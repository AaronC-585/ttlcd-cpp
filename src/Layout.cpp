// src/Layout.cpp
#include "Layout.hpp"
#include <iostream>

Layout::Layout(const json& config) : config_(config) {
    background_path_ = config_["background"].get<std::string>();

    ft2_ = cv::freetype::createFreeType2();
    if (config_.contains("font_file") && !config_["font_file"].get<std::string>().empty()) {
        std::string font_path = config_["font_file"].get<std::string>();
        ft2_->loadFontData(font_path, 0);
        std::cout << "Loaded custom font: " << font_path << "\n";
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

std::string Layout::render(const std::string& output_path) {
    if (background_.empty()) load_background();

    cv::Mat frame = background_.clone();

    for (auto& widget : widgets_) {
        widget->draw(frame, this);
    }

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(480, 128));

    std::string orient = config_.value("orientation", "TOP");
    if (orient == "LEFT") cv::rotate(resized, resized, cv::ROTATE_90_CLOCKWISE);
    else if (orient == "BOTTOM") cv::rotate(resized, resized, cv::ROTATE_180);
    else if (orient == "RIGHT") cv::rotate(resized, resized, cv::ROTATE_90_COUNTERCLOCKWISE);

    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};
    if (!cv::imwrite(output_path, resized, params)) {
        throw std::runtime_error("Failed to save image: " + output_path);
    }

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
            } else if (type == "gpu_memory") {
                widget = std::make_unique<GpuMemoryWidget>(w_config);
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
