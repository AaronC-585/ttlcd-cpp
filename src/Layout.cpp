// src/Layout.cpp
#include "Layout.hpp"
#include "EmbeddedFont.hpp"
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Thermaltake panels expect baseline JPEG at 480x128 with 300 DPI (see reference ttlcd.py).
void patch_jpeg_dpi(std::vector<uint8_t>& data, int dpi) {
    for (size_t i = 0; i + 15 < data.size(); ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xE0 &&
            data[i + 4] == 'J' && data[i + 5] == 'F' &&
            data[i + 6] == 'I' && data[i + 7] == 'F') {
            data[i + 11] = 0x01; // dots per inch
            data[i + 12] = static_cast<uint8_t>((dpi >> 8) & 0xFF);
            data[i + 13] = static_cast<uint8_t>(dpi & 0xFF);
            data[i + 14] = static_cast<uint8_t>((dpi >> 8) & 0xFF);
            data[i + 15] = static_cast<uint8_t>(dpi & 0xFF);
            break;
        }
    }
}

}  // namespace

int Layout::jpeg_dpi() const {
    return config_.value("jpeg_dpi", DEFAULT_JPEG_DPI);
}

int Layout::render_width() const {
    return std::max(1, static_cast<int>(std::lround(
        LCD_WIDTH * static_cast<double>(jpeg_dpi()) / DESIGN_JPEG_DPI)));
}

int Layout::render_height() const {
    return std::max(1, static_cast<int>(std::lround(
        LCD_HEIGHT * static_cast<double>(jpeg_dpi()) / DESIGN_JPEG_DPI)));
}

int Layout::scale_design_x(int x) const {
    return static_cast<int>(std::lround(
        x * static_cast<double>(render_width()) / LCD_WIDTH));
}

int Layout::scale_design_y(int y) const {
    return static_cast<int>(std::lround(
        y * static_cast<double>(render_height()) / LCD_HEIGHT));
}

int Layout::scale_design_size(int px) const {
    return static_cast<int>(std::lround(
        px * static_cast<double>(render_width()) / LCD_WIDTH));
}

double Layout::get_font_scale_factor() const {
    double canvas_scale = 1.0;
    if (!background_.empty()) {
        canvas_scale = static_cast<double>(render_width()) / LCD_WIDTH;
    } else if (config_.contains("design_width")) {
        canvas_scale = config_["design_width"].get<double>() / LCD_WIDTH;
    }

    const double dpi_scale = static_cast<double>(jpeg_dpi()) / REFERENCE_DPI;
    const double percent = config_.value("font_scale_percent", 100.0) / 100.0;
    return canvas_scale * dpi_scale * percent;
}

int Layout::scale_font_size(int base_size) const {
    return std::max(1, static_cast<int>(std::lround(base_size * get_font_scale_factor())));
}

Layout::Layout(const json& config) : config_(config) {
    background_path_ = config_["background"].get<std::string>();

    ft2_ = cv::freetype::createFreeType2();

    const std::string font_file = config_.value("font_file", "embedded");
    if (font_file == "embedded") {
        font_data_.assign(EmbeddedFont::comic_ttf,
                          EmbeddedFont::comic_ttf + EmbeddedFont::comic_ttf_size);
        ft2_->loadFontData(reinterpret_cast<char*>(font_data_.data()),
                           font_data_.size(), 0);
        std::cout << "Loaded embedded font (comic.ttf, "
                  << font_data_.size() << " bytes)\n";
    } else if (!font_file.empty()) {
        if (std::filesystem::exists(font_file)) {
            ft2_->loadFontData(font_file, 0);
            std::cout << "Loaded custom font: " << font_file << "\n";
        } else {
            std::cerr << "Warning: font file not found: " << font_file
                      << " — falling back to embedded font\n";
            font_data_.assign(EmbeddedFont::comic_ttf,
                              EmbeddedFont::comic_ttf + EmbeddedFont::comic_ttf_size);
            ft2_->loadFontData(reinterpret_cast<char*>(font_data_.data()),
                               font_data_.size(), 0);
        }
    }
}

void Layout::load_background() {
    background_ = cv::imread(background_path_);
    if (background_.empty()) {
        throw std::runtime_error("Failed to load background: " + background_path_);
    }
}

void Layout::add_widget(std::unique_ptr<Widget> widget) {
    widgets_.push_back(std::move(widget));
}

void Layout::refresh_widgets() {
    for (auto& widget : widgets_) {
        widget->tick();
    }
}

void Layout::warmup() {
    if (background_.empty()) {
        load_background();
    }
    refresh_widgets();

    cv::Mat frame;
    cv::resize(background_, frame, cv::Size(render_width(), render_height()));
    for (auto& widget : widgets_) {
        widget->draw(frame, this);
    }
}

cv::Mat Layout::compose_frame() {
    if (background_.empty()) {
        throw std::runtime_error("Background not loaded");
    }

    const int rw = render_width();
    const int rh = render_height();

    cv::Mat frame;
    cv::resize(background_, frame, cv::Size(rw, rh));

    for (auto& widget : widgets_) {
        widget->draw(frame, this);
    }

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(LCD_WIDTH, LCD_HEIGHT));

    if (resized.cols != LCD_WIDTH || resized.rows != LCD_HEIGHT) {
        throw std::runtime_error("Frame resize failed: expected " +
                                 std::to_string(LCD_WIDTH) + "x" + std::to_string(LCD_HEIGHT));
    }

    std::string orient = config_.value("orientation", "TOP");
    if (orient == "LEFT") {
        cv::rotate(resized, resized, cv::ROTATE_90_CLOCKWISE);
    } else if (orient == "BOTTOM") {
        cv::rotate(resized, resized, cv::ROTATE_180);
    } else if (orient == "RIGHT") {
        cv::rotate(resized, resized, cv::ROTATE_90_COUNTERCLOCKWISE);
    }

    return resized;
}

std::vector<uint8_t> Layout::encode_jpeg(const cv::Mat& image) const {
    std::vector<uint8_t> buffer;
    const std::vector<int> params = {
        cv::IMWRITE_JPEG_QUALITY, config_.value("jpeg_quality", 80),
        cv::IMWRITE_JPEG_OPTIMIZE, config_.value("jpeg_optimize", false) ? 1 : 0,
        cv::IMWRITE_JPEG_PROGRESSIVE, 0,
    };
    if (!cv::imencode(".jpg", image, buffer, params)) {
        throw std::runtime_error("Failed to encode JPEG");
    }
    return buffer;
}

std::vector<uint8_t> Layout::render_jpeg() {
    if (background_.empty()) {
        load_background();
    }

    refresh_widgets();

    const cv::Mat frame = compose_frame();
    std::vector<uint8_t> jpeg = encode_jpeg(frame);
    patch_jpeg_dpi(jpeg, jpeg_dpi());

    if (jpeg.size() < 4 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) {
        throw std::runtime_error("JPEG encode did not produce a valid image");
    }

    return jpeg;
}

std::string Layout::render(const std::string& output_path) {
    const std::vector<uint8_t> jpeg = render_jpeg();
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to save image: " + output_path);
    }
    out.write(reinterpret_cast<const char*>(jpeg.data()), static_cast<std::streamsize>(jpeg.size()));
    return output_path;
}
/*
// NodeLayout
NodeLayout::NodeLayout(const json& config) : Layout(config) {}*/

bool NodeLayout::setup() {
    try {
        const auto& widget_list = config_["widgets"];
        if (!widget_list.is_array()) {
            throw std::runtime_error("Missing or invalid 'widgets' array");
        }

        for (const auto& w_config : widget_list) {
            if (!w_config.value("enable", true)) continue;

            std::string type = w_config["type"].get<std::string>();

            std::unique_ptr<Widget> widget;

            // Basic widgets
            if (type == "date") {
                widget = std::make_unique<DateWidget>(w_config);
            } else if (type == "time") {
                widget = std::make_unique<TimeWidget>(w_config);
            } else if (type == "text") {
                widget = std::make_unique<TextWidget>(w_config);
            } else if (type == "line") {
                widget = std::make_unique<LineWidget>(w_config);
            } else if (type == "dynamic_text") {
                widget = std::make_unique<DynamicTextWidget>(w_config);
            }
            // CPU widgets
            else if (type == "cpu_bar") {
                widget = std::make_unique<CpuUtilizationWidget>(w_config);
                widget->set_type(WidgetType::Bar);
                widget->set_bar_orientation(BarOrientation::Horizontal);
                widget->set_bar_direction(w_config.value("direction", "right"));
                widget->set_bar_size(w_config.value("width", 440), w_config.value("height", 30));
            } else if (type == "cpu_core") {
                widget = std::make_unique<CpuCoreWidget>(w_config);
            } else if (type == "cpu_freq") {
                widget = std::make_unique<CpuFreqWidget>(w_config);
            } else if (type == "cpu_temp") {
                widget = std::make_unique<CpuTempWidget>(w_config);
            }
            // Memory widgets
            else if (type == "ram_bar") {
                widget = std::make_unique<RamUtilizationBarWidget>(w_config);
                widget->set_type(WidgetType::Bar);
                widget->set_bar_orientation(BarOrientation::Horizontal);
                widget->set_bar_size(w_config.value("width", 440), w_config.value("height", 30));
            } else if (type == "memory_details") {
                widget = std::make_unique<MemoryDetailsWidget>(w_config);
            } else if (type == "swap_usage") {
                widget = std::make_unique<SwapUsageWidget>(w_config);
            }
            // Network widgets
            else if (type == "network") {
                widget = std::make_unique<NetworkSpeedWidget>(w_config);
            } else if (type == "network_interface") {
                widget = std::make_unique<NetworkInterfaceWidget>(w_config);
            }
            // Disk widgets
            else if (type == "disk_usage") {
                widget = std::make_unique<DiskUsageWidget>(w_config);
            } else if (type == "disk_io") {
                widget = std::make_unique<DiskIOWidget>(w_config);
            }
            // GPU widgets
            else if (type == "gpu_temp") {
                widget = std::make_unique<GpuTempWidget>(w_config);
            } else if (type == "gpu_usage") {
                widget = std::make_unique<GpuUsageWidget>(w_config);
            }             else if (type == "gpu_memory") {
                widget = std::make_unique<GpuMemoryWidget>(w_config);
            } else if (type == "all_temps") {
                widget = std::make_unique<AllTempSensorsWidget>(w_config);
            }
            // System widgets
            else if (type == "loadavg") {
                widget = std::make_unique<LoadAverageWidget>(w_config);
            } else if (type == "uptime") {
                widget = std::make_unique<UptimeWidget>(w_config);
            } else if (type == "process_count") {
                widget = std::make_unique<ProcessCountWidget>(w_config);
            } else if (type == "fan_speed") {
                widget = std::make_unique<FanSpeedWidget>(w_config);
            } else if (type == "battery") {
                widget = std::make_unique<BatteryWidget>(w_config);
            } else {
                std::cerr << "Unknown widget type: " << type << "\n";
                continue;
            }

            widget->set_position(w_config["x"].get<int>(), w_config["y"].get<int>());

            // Fixed font size default
            int global_font_size = config_.value("font_size", 14);
            widget->set_font_size(w_config.value("font_size", global_font_size));

            if (w_config.contains("font_color")) {
                auto c = w_config["font_color"];
                widget->set_font_color(cv::Scalar(c[2], c[1], c[0]));
            }

            add_widget(std::move(widget));
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Layout setup failed: " << e.what() << std::endl;
        return false;
    }
}
