// src/Widget.cpp - With Full Error Handling

#include "Widget.hpp"
#include "Layout.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <filesystem>
#include <iostream>

Widget::Widget(const json& config) : config_(config) {
    try {
        if (config.contains("font_size")) font_size_ = config["font_size"].get<int>();
        if (config.contains("font_color") && config["font_color"].is_array()) {
            auto arr = config["font_color"];
            font_color_ = cv::Scalar(arr[2], arr[1], arr[0]);
        }
    } catch (const std::exception& e) {
        std::cerr << "Widget config error: " << e.what() << " — using defaults\n";
    }
}

void Widget::draw(cv::Mat& image, Layout* layout) {
    tick();

    if (layout && !layout->get_ft2().empty()) {
        layout->get_ft2()->putText(image, value_, cv::Point(x_, y_), font_size_,
                                   font_color_, -1, cv::LINE_AA, true);
    } else {
        cv::putText(image, value_, cv::Point(x_, y_ + font_size_),
                    cv::FONT_HERSHEY_SIMPLEX, font_size_ / 24.0,
                    font_color_, 1, cv::LINE_AA);
    }

    try {
        if (layout && !layout->ft2_.empty()) {
            layout->ft2_->putText(image, value_, cv::Point(x_, y_), font_size_,
                                  font_color_, -1, cv::LINE_AA, true);
        } else {
            cv::putText(image, value_, cv::Point(x_, y_ + font_size_),
                        cv::FONT_HERSHEY_SIMPLEX, font_size_ / 24.0,
                        font_color_, 1, cv::LINE_AA);
        }
    } catch (const std::exception& e) {
        std::cerr << "Widget draw error (using fallback): " << e.what() << "\n";
        cv::putText(image, value_, cv::Point(x_, y_ + font_size_),
                    cv::FONT_HERSHEY_SIMPLEX, font_size_ / 24.0,
                    cv::Scalar(0, 0, 255), 1, cv::LINE_AA);  // Red on error
    }

    // Bar rendering with error handling
    if (type_ == WidgetType::Bar) {
        double percent = 0.0;
        try {
            percent = std::stod(value_);
        } catch (...) {
            percent = 0.0;
            std::cerr << "Invalid bar value: '" << value_ << "' — using 0%\n";
        }
        percent = std::min(100.0, std::max(0.0, percent));

        int filled = static_cast<int>((percent / 100.0) *
            (bar_ori_ == BarOrientation::Horizontal ? bar_width_ : bar_height_));

        cv::Rect bg_rect(x_, y_,
                         bar_ori_ == BarOrientation::Horizontal ? bar_width_ : bar_height_,
                         bar_ori_ == BarOrientation::Horizontal ? bar_height_ : bar_width_);

        try {
            cv::rectangle(image, bg_rect, cv::Scalar(30, 30, 30), cv::FILLED);
            cv::rectangle(image, bg_rect, bar_outline_, 2);

            cv::Rect fill_rect = bg_rect;
            if (bar_ori_ == BarOrientation::Horizontal) {
                if (bar_direction_ == "right") fill_rect.width = filled;
                else if (bar_direction_ == "left") { fill_rect.x += bar_width_ - filled; fill_rect.width = filled; }
            } else {
                if (bar_direction_ == "down") fill_rect.height = filled;
                else if (bar_direction_ == "up") { fill_rect.y += bar_height_ - filled; fill_rect.height = filled; }
            }
            cv::rectangle(image, fill_rect, bar_fill_, cv::FILLED);
        } catch (const std::exception& e) {
            std::cerr << "Bar drawing failed: " << e.what() << "\n";
        }
    }
}

// DateWidget
void DateWidget::tick() {
    try {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        value_ = oss.str();
    } catch (...) {
        value_ = "DATE ERR";
    }
}

// TimeWidget
void TimeWidget::tick() {
    try {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        value_ = oss.str();
    } catch (...) {
        value_ = "TIME ERR";
    }
}

// LoadAverageWidget
LoadAverageWidget::LoadAverageWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

void LoadAverageWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;

    try {
        std::ifstream f("/proc/loadavg");
        if (!f) throw std::runtime_error("Cannot open /proc/loadavg");

        double l1, l5, l15;
        int running, total;
        unsigned long pid;

        if (!(f >> l1 >> l5 >> l15 >> running >> total >> pid)) {
            throw std::runtime_error("Failed to parse loadavg");
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << l1 << " " << l5 << " " << l15;
        value_ = oss.str();
    } catch (const std::exception& e) {
        std::cerr << "LoadAverage error: " << e.what() << "\n";
        value_ = "LOAD ERR";
    }

    last_tick_ = now;
}

// CpuUtilizationWidget
CpuUtilizationWidget::CpuUtilizationWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

CpuUtilizationWidget::CpuTimes CpuUtilizationWidget::read_cpu_times() {
    CpuTimes t;
    try {
        std::ifstream f("/proc/stat");
        if (!f) throw std::runtime_error("Cannot open /proc/stat");

        std::string line;
        if (!std::getline(f, line) || line.rfind("cpu ", 0) != 0) {
            throw std::runtime_error("Invalid cpu line");
        }

        std::istringstream iss(line.substr(4));
        iss >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
    } catch (...) {
        std::cerr << "CPU stats read failed\n";
    }
    return t;
}

void CpuUtilizationWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;

    try {
        auto curr = read_cpu_times();
        if (!initialized_) {
            prev_ = curr;
            initialized_ = true;
            value_ = "0";
            last_tick_ = now;
            return;
        }

        auto total_d = curr.total() - prev_.total();
        auto idle_d = curr.idle_time() - prev_.idle_time();
        double util = total_d ? 100.0 * (total_d - idle_d) / total_d : 0.0;
        value_ = std::to_string(static_cast<int>(util));

        prev_ = curr;
    } catch (const std::exception& e) {
        std::cerr << "CPU tick error: " << e.what() << "\n";
        value_ = "CPU ERR";
    }

    last_tick_ = now;
}

// RamUtilizationBarWidget
RamUtilizationBarWidget::RamUtilizationBarWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

void RamUtilizationBarWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;

    try {
        struct sysinfo info;
        if (sysinfo(&info) != 0) throw std::runtime_error("sysinfo failed");

        unsigned long long total = (unsigned long long)info.totalram * info.mem_unit;
        unsigned long long free = (unsigned long long)info.freeram * info.mem_unit;
        unsigned long long buffers = (unsigned long long)info.bufferram * info.mem_unit;
        unsigned long long available = free + buffers;

        double used_percent = total ? 100.0 * (total - available) / total : 0.0;
        value_ = std::to_string(static_cast<int>(used_percent));
    } catch (const std::exception& e) {
        std::cerr << "RAM tick error: " << e.what() << "\n";
        value_ = "RAM ERR";
    }

    last_tick_ = now;
}

// NetworkSpeedWidget
NetworkSpeedWidget::NetworkSpeedWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_), initialized_(false) {}

NetworkSpeedWidget::NetStats NetworkSpeedWidget::read_net_stats() {
    NetStats s{};
    try {
        std::ifstream f("/proc/net/dev");
        if (!f) throw std::runtime_error("Cannot open /proc/net/dev");

        std::string line;
        while (std::getline(f, line)) {
            if (line.find("eth") != std::string::npos ||
                line.find("en") != std::string::npos ||
                line.find("wl") != std::string::npos) {
                std::istringstream iss(line);
                std::string iface;
                iss >> iface >> s.rx;
                for (int i = 0; i < 7; ++i) { uint64_t dummy; iss >> dummy; }
                iss >> s.tx;
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Net stats read error: " << e.what() << "\n";
    }
    return s;
}

void NetworkSpeedWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    double diff_sec = std::chrono::duration<double>(now - last_tick_).count();
    if (diff_sec < 1.0) return;

    try {
        auto curr = read_net_stats();
        if (!initialized_) {
            prev_ = curr;
            initialized_ = true;
            value_ = "↑ 0 KB/s\n↓ 0 KB/s";
            last_tick_ = now;
            return;
        }

        double tx_kbs = (curr.tx - prev_.tx) / diff_sec / 1024.0;
        double rx_kbs = (curr.rx - prev_.rx) / diff_sec / 1024.0;

        std::ostringstream oss;
        oss << "↑ " << std::fixed << std::setprecision(1) << tx_kbs << " KB/s\n"
            << "↓ " << rx_kbs << " KB/s";
        value_ = oss.str();

        prev_ = curr;
    } catch (const std::exception& e) {
        std::cerr << "Network tick error: " << e.what() << "\n";
        value_ = "NET ERR";
    }

    last_tick_ = now;
}

// DynamicTextWidget
DynamicTextWidget::DynamicTextWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    try {
        format_string_ = config.value("format", "Dynamic: {value}");
    } catch (...) {
        format_string_ = "Dynamic Error";
    }
    value_ = format_string_;
}

std::string DynamicTextWidget::get_current_time() {
    try {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    } catch (...) {
        return "TIME ERR";
    }
}

std::string DynamicTextWidget::get_dynamic_value() {
    static int counter = 0;
    counter++;
    std::ostringstream oss;
    oss << "Frame " << counter << " | " << get_current_time();
    return oss.str();
}

void DynamicTextWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;

    try {
        std::string dynamic = get_dynamic_value();

        size_t pos = format_string_.find("{value}");
        if (pos != std::string::npos) {
            value_ = format_string_.substr(0, pos) + dynamic + format_string_.substr(pos + 7);
        } else {
            value_ = dynamic;
        }
    } catch (const std::exception& e) {
        std::cerr << "DynamicText tick error: " << e.what() << "\n";
        value_ = "DYN ERR";
    }

    last_tick_ = now;
}

TextWidget::TextWidget(const json& config) : Widget(config) {
    static_text_ = config.value("text", "Static Text");
    value_ = static_text_;
}

void TextWidget::tick() {
    // Static — no change
    // Could add dynamic features later
}

// LineWidget
LineWidget::LineWidget(const json& config) : Widget(config) {
    try {
        orientation_ = config.value("orientation", "horizontal");
        length_ = config.value("length", 100);
        thickness_ = config.value("thickness", 1);
        
        if (config.contains("line_color") && config["line_color"].is_array()) {
            auto arr = config["line_color"];
            line_color_ = cv::Scalar(arr[2], arr[1], arr[0]);
        } else if (config.contains("font_color") && config["font_color"].is_array()) {
            // Fallback to font_color if line_color not specified
            auto arr = config["font_color"];
            line_color_ = cv::Scalar(arr[2], arr[1], arr[0]);
        }
        
        // Calculate end point based on orientation
        if (orientation_ == "horizontal") {
            x2_ = x_ + length_;
            y2_ = y_;
        } else if (orientation_ == "vertical") {
            x2_ = x_;
            y2_ = y_ + length_;
        } else {
            // Custom end points if specified
            x2_ = config.value("x2", x_ + length_);
            y2_ = config.value("y2", y_);
        }
    } catch (const std::exception& e) {
        std::cerr << "LineWidget config error: " << e.what() << " – using defaults\n";
        x2_ = x_ + 100;
        y2_ = y_;
    }
}

void LineWidget::tick() {
    // Lines don't need updates
    value_ = "";
}

void LineWidget::draw(cv::Mat& image, Layout* layout) {
    (void)layout;  // Unused parameter
    
    try {
        cv::line(image, cv::Point(x_, y_), cv::Point(x2_, y2_), 
                line_color_, thickness_, cv::LINE_AA);
    } catch (const std::exception& e) {
        std::cerr << "LineWidget draw error: " << e.what() << "\n";
    }
}

// ============================================================================
// TEMPERATURE SENSORS
// ============================================================================

// CpuTempWidget
CpuTempWidget::CpuTempWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    sensor_path_ = config.value("sensor_path", "/sys/class/thermal/thermal_zone0/temp");
}

double CpuTempWidget::read_temperature() {
    try {
        std::ifstream file(sensor_path_);
        if (!file) {
            // Try alternative paths
            std::vector<std::string> alt_paths = {
                "/sys/class/hwmon/hwmon0/temp1_input",
                "/sys/class/hwmon/hwmon1/temp1_input",
                "/sys/class/hwmon/hwmon2/temp1_input"
            };
            for (const auto& path : alt_paths) {
                file.open(path);
                if (file) break;
            }
            if (!file) return -999.0;
        }
        
        long temp_raw;
        file >> temp_raw;
        return temp_raw / 1000.0;  // Convert millidegrees to degrees
    } catch (...) {
        return -999.0;
    }
}

void CpuTempWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        double temp = read_temperature();
        if (temp > -900.0) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << temp << "°C";
            value_ = oss.str();
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "CpuTemp error: " << e.what() << "\n";
        value_ = "TEMP ERR";
    }
    
    last_tick_ = now;
}

// GpuTempWidget
GpuTempWidget::GpuTempWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    gpu_type_ = config.value("gpu_type", "nvidia");
}

double GpuTempWidget::read_nvidia_temp() {
    try {
        FILE* pipe = popen("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
        if (!pipe) return -999.0;
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        if (!result.empty()) {
            return std::stod(result);
        }
    } catch (...) {}
    return -999.0;
}

double GpuTempWidget::read_amd_temp() {
    try {
        // Try AMD GPU sysfs path
        std::ifstream file("/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input");
        if (file) {
            long temp;
            file >> temp;
            return temp / 1000.0;
        }
    } catch (...) {}
    return -999.0;
}

void GpuTempWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        double temp = -999.0;
        if (gpu_type_ == "nvidia") {
            temp = read_nvidia_temp();
        } else if (gpu_type_ == "amd") {
            temp = read_amd_temp();
        }
        
        if (temp > -900.0) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << temp << "°C";
            value_ = oss.str();
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "GpuTemp error: " << e.what() << "\n";
        value_ = "GPU ERR";
    }
    
    last_tick_ = now;
}

// ============================================================================
// DISK SENSORS
// ============================================================================

// DiskUsageWidget
DiskUsageWidget::DiskUsageWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    mount_point_ = config.value("mount_point", "/");
}

DiskUsageWidget::DiskStats DiskUsageWidget::read_disk_usage() {
    DiskStats stats;
    try {
        struct statvfs vfs;
        if (statvfs(mount_point_.c_str(), &vfs) == 0) {
            stats.total = vfs.f_blocks * vfs.f_frsize;
            stats.available = vfs.f_bavail * vfs.f_frsize;
            stats.used = stats.total - (vfs.f_bfree * vfs.f_frsize);
            stats.percent = stats.total > 0 ? (stats.used * 100.0 / stats.total) : 0.0;
        }
    } catch (...) {}
    return stats;
}

void DiskUsageWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        auto stats = read_disk_usage();
        std::ostringstream oss;
        
        double used_gb = stats.used / (1024.0 * 1024.0 * 1024.0);
        double total_gb = stats.total / (1024.0 * 1024.0 * 1024.0);
        
        oss << std::fixed << std::setprecision(1) 
            << used_gb << "/" << total_gb << " GB (" 
            << std::setprecision(0) << stats.percent << "%)";
        value_ = oss.str();
    } catch (const std::exception& e) {
        std::cerr << "DiskUsage error: " << e.what() << "\n";
        value_ = "DISK ERR";
    }
    
    last_tick_ = now;
}

// DiskIOWidget
DiskIOWidget::DiskIOWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    device_ = config.value("device", "sda");
}

DiskIOWidget::IOStats DiskIOWidget::read_disk_io() {
    IOStats stats;
    try {
        std::ifstream file("/proc/diskstats");
        std::string line;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            int major, minor;
            std::string dev_name;
            uint64_t reads, reads_merged, sectors_read, read_time;
            uint64_t writes, writes_merged, sectors_written, write_time;
            
            iss >> major >> minor >> dev_name;
            
            if (dev_name == device_) {
                iss >> reads >> reads_merged >> sectors_read >> read_time
                    >> writes >> writes_merged >> sectors_written >> write_time;
                
                stats.read_bytes = sectors_read * 512;
                stats.write_bytes = sectors_written * 512;
                break;
            }
        }
    } catch (...) {}
    return stats;
}

void DiskIOWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    double diff_sec = std::chrono::duration<double>(now - last_tick_).count();
    if (diff_sec < 1.0) return;
    
    try {
        auto curr = read_disk_io();
        if (!initialized_) {
            prev_ = curr;
            initialized_ = true;
            value_ = "R: 0 MB/s\nW: 0 MB/s";
            last_tick_ = now;
            return;
        }
        
        double read_mbs = (curr.read_bytes - prev_.read_bytes) / diff_sec / (1024.0 * 1024.0);
        double write_mbs = (curr.write_bytes - prev_.write_bytes) / diff_sec / (1024.0 * 1024.0);
        
        std::ostringstream oss;
        oss << "R: " << std::fixed << std::setprecision(1) << read_mbs << " MB/s\n"
            << "W: " << write_mbs << " MB/s";
        value_ = oss.str();
        
        prev_ = curr;
    } catch (const std::exception& e) {
        std::cerr << "DiskIO error: " << e.what() << "\n";
        value_ = "IO ERR";
    }
    
    last_tick_ = now;
}

// ============================================================================
// CPU DETAILED SENSORS
// ============================================================================

// CpuCoreWidget
CpuCoreWidget::CpuCoreWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    core_id_ = config.value("core_id", -1);  // -1 = all cores
}

std::vector<CpuCoreWidget::CoreTimes> CpuCoreWidget::read_core_times() {
    std::vector<CoreTimes> cores;
    try {
        std::ifstream file("/proc/stat");
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.rfind("cpu", 0) == 0 && line[3] >= '0' && line[3] <= '9') {
                CoreTimes t;
                std::istringstream iss(line.substr(line.find(' ')));
                iss >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
                cores.push_back(t);
            }
        }
    } catch (...) {}
    return cores;
}

void CpuCoreWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        auto curr_cores = read_core_times();
        if (!initialized_) {
            prev_cores_ = curr_cores;
            initialized_ = true;
            value_ = "0%";
            last_tick_ = now;
            return;
        }
        
        if (core_id_ >= 0 && core_id_ < static_cast<int>(curr_cores.size())) {
            // Single core
            auto& curr = curr_cores[core_id_];
            auto& prev = prev_cores_[core_id_];
            auto total_d = curr.total() - prev.total();
            auto idle_d = curr.idle_time() - prev.idle_time();
            double util = total_d ? 100.0 * (total_d - idle_d) / total_d : 0.0;
            value_ = std::to_string(static_cast<int>(util)) + "%";
        } else {
            // All cores
            std::ostringstream oss;
            for (size_t i = 0; i < curr_cores.size() && i < 8; ++i) {
                if (i >= prev_cores_.size()) break;
                auto total_d = curr_cores[i].total() - prev_cores_[i].total();
                auto idle_d = curr_cores[i].idle_time() - prev_cores_[i].idle_time();
                double util = total_d ? 100.0 * (total_d - idle_d) / total_d : 0.0;
                oss << static_cast<int>(util) << "%";
                if (i < curr_cores.size() - 1) oss << " ";
            }
            value_ = oss.str();
        }
        
        prev_cores_ = curr_cores;
    } catch (const std::exception& e) {
        std::cerr << "CpuCore error: " << e.what() << "\n";
        value_ = "CORE ERR";
    }
    
    last_tick_ = now;
}

// CpuFreqWidget
CpuFreqWidget::CpuFreqWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    core_id_ = config.value("core_id", 0);
}

double CpuFreqWidget::read_cpu_freq() {
    try {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(core_id_) + "/cpufreq/scaling_cur_freq";
        std::ifstream file(path);
        if (file) {
            uint64_t freq_khz;
            file >> freq_khz;
            return freq_khz / 1000.0;  // Convert to MHz
        }
    } catch (...) {}
    return 0.0;
}

void CpuFreqWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        double freq_mhz = read_cpu_freq();
        if (freq_mhz > 0) {
            std::ostringstream oss;
            if (freq_mhz >= 1000) {
                oss << std::fixed << std::setprecision(2) << (freq_mhz / 1000.0) << " GHz";
            } else {
                oss << std::fixed << std::setprecision(0) << freq_mhz << " MHz";
            }
            value_ = oss.str();
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "CpuFreq error: " << e.what() << "\n";
        value_ = "FREQ ERR";
    }
    
    last_tick_ = now;
}

// ============================================================================
// MEMORY SENSORS
// ============================================================================

// MemoryDetailsWidget
MemoryDetailsWidget::MemoryDetailsWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    display_mode_ = config.value("display_mode", "used");
}

MemoryDetailsWidget::MemInfo MemoryDetailsWidget::read_meminfo() {
    MemInfo info;
    try {
        std::ifstream file("/proc/meminfo");
        std::string line;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            std::string unit;
            
            iss >> key >> value >> unit;
            
            if (key == "MemTotal:") info.total = value * 1024;
            else if (key == "MemFree:") info.free = value * 1024;
            else if (key == "MemAvailable:") info.available = value * 1024;
            else if (key == "Buffers:") info.buffers = value * 1024;
            else if (key == "Cached:") info.cached = value * 1024;
            else if (key == "SwapTotal:") info.swap_total = value * 1024;
            else if (key == "SwapFree:") info.swap_free = value * 1024;
        }
    } catch (...) {}
    return info;
}

void MemoryDetailsWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        auto info = read_meminfo();
        std::ostringstream oss;
        
        double gb_val = 0.0;
        if (display_mode_ == "total") {
            gb_val = info.total / (1024.0 * 1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << gb_val << " GB";
        } else if (display_mode_ == "used") {
            uint64_t used = info.total - info.available;
            gb_val = used / (1024.0 * 1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << gb_val << " GB";
        } else if (display_mode_ == "free") {
            gb_val = info.free / (1024.0 * 1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << gb_val << " GB";
        } else if (display_mode_ == "available") {
            gb_val = info.available / (1024.0 * 1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << gb_val << " GB";
        } else if (display_mode_ == "cached") {
            gb_val = info.cached / (1024.0 * 1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << gb_val << " GB";
        }
        
        value_ = oss.str();
    } catch (const std::exception& e) {
        std::cerr << "MemoryDetails error: " << e.what() << "\n";
        value_ = "MEM ERR";
    }
    
    last_tick_ = now;
}

// SwapUsageWidget
SwapUsageWidget::SwapUsageWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

void SwapUsageWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        std::ifstream file("/proc/meminfo");
        std::string line;
        uint64_t swap_total = 0, swap_free = 0;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            
            iss >> key >> value;
            if (key == "SwapTotal:") swap_total = value;
            else if (key == "SwapFree:") swap_free = value;
        }
        
        if (swap_total > 0) {
            uint64_t swap_used = swap_total - swap_free;
            double percent = 100.0 * swap_used / swap_total;
            value_ = std::to_string(static_cast<int>(percent));
        } else {
            value_ = "0";
        }
    } catch (const std::exception& e) {
        std::cerr << "SwapUsage error: " << e.what() << "\n";
        value_ = "SWAP ERR";
    }
    
    last_tick_ = now;
}

// ============================================================================
// SYSTEM SENSORS
// ============================================================================

// ProcessCountWidget
ProcessCountWidget::ProcessCountWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

int ProcessCountWidget::count_processes() {
    try {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (!name.empty() && std::isdigit(name[0])) {
                    count++;
                }
            }
        }
        return count;
    } catch (...) {
        return -1;
    }
}

void ProcessCountWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        int count = count_processes();
        if (count >= 0) {
            value_ = std::to_string(count) + " procs";
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "ProcessCount error: " << e.what() << "\n";
        value_ = "PROC ERR";
    }
    
    last_tick_ = now;
}

// UptimeWidget
UptimeWidget::UptimeWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {}

std::string UptimeWidget::format_uptime(long seconds) {
    long days = seconds / 86400;
    long hours = (seconds % 86400) / 3600;
    long mins = (seconds % 3600) / 60;
    
    std::ostringstream oss;
    if (days > 0) {
        oss << days << "d " << hours << "h";
    } else if (hours > 0) {
        oss << hours << "h " << mins << "m";
    } else {
        oss << mins << "m";
    }
    return oss.str();
}

void UptimeWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        std::ifstream file("/proc/uptime");
        double uptime_seconds;
        file >> uptime_seconds;
        
        value_ = format_uptime(static_cast<long>(uptime_seconds));
    } catch (const std::exception& e) {
        std::cerr << "Uptime error: " << e.what() << "\n";
        value_ = "UP ERR";
    }
    
    last_tick_ = now;
}

// FanSpeedWidget
FanSpeedWidget::FanSpeedWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    sensor_path_ = config.value("sensor_path", "/sys/class/hwmon/hwmon0/fan1_input");
}

int FanSpeedWidget::read_fan_speed() {
    try {
        std::ifstream file(sensor_path_);
        if (file) {
            int rpm;
            file >> rpm;
            return rpm;
        }
    } catch (...) {}
    return -1;
}

void FanSpeedWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        int rpm = read_fan_speed();
        if (rpm >= 0) {
            value_ = std::to_string(rpm) + " RPM";
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "FanSpeed error: " << e.what() << "\n";
        value_ = "FAN ERR";
    }
    
    last_tick_ = now;
}

// BatteryWidget
BatteryWidget::BatteryWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    battery_path_ = config.value("battery_path", "/sys/class/power_supply/BAT0");
}

BatteryWidget::BatteryInfo BatteryWidget::read_battery() {
    BatteryInfo info;
    try {
        std::ifstream cap_file(battery_path_ + "/capacity");
        std::ifstream status_file(battery_path_ + "/status");
        
        if (cap_file) cap_file >> info.percent;
        if (status_file) std::getline(status_file, info.status);
        
        info.charging = (info.status == "Charging");
    } catch (...) {}
    return info;
}

void BatteryWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        auto info = read_battery();
        std::ostringstream oss;
        oss << info.percent << "% ";
        if (info.charging) {
            oss << "⚡";
        }
        value_ = oss.str();
    } catch (const std::exception& e) {
        std::cerr << "Battery error: " << e.what() << "\n";
        value_ = "BAT ERR";
    }
    
    last_tick_ = now;
}

// NetworkInterfaceWidget
NetworkInterfaceWidget::NetworkInterfaceWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    interface_ = config.value("interface", "eth0");
    display_mode_ = config.value("display_mode", "status");
}

std::string NetworkInterfaceWidget::read_interface_info() {
    try {
        if (display_mode_ == "status") {
            std::string path = "/sys/class/net/" + interface_ + "/operstate";
            std::ifstream file(path);
            if (file) {
                std::string state;
                std::getline(file, state);
                return state;
            }
        } else if (display_mode_ == "ip") {
            std::string cmd = "ip -4 addr show " + interface_ + " | grep -oP '(?<=inet\\s)\\d+(\\.\\d+){3}' 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[128];
                std::string result;
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    result = buffer;
                    // Trim newline
                    if (!result.empty() && result.back() == '\n') {
                        result.pop_back();
                    }
                }
                pclose(pipe);
                return result.empty() ? "No IP" : result;
            }
        }
    } catch (...) {}
    return "N/A";
}

void NetworkInterfaceWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        value_ = read_interface_info();
    } catch (const std::exception& e) {
        std::cerr << "NetworkInterface error: " << e.what() << "\n";
        value_ = "NET ERR";
    }
    
    last_tick_ = now;
}

// ============================================================================
// GPU SENSORS
// ============================================================================

// GpuUsageWidget
GpuUsageWidget::GpuUsageWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    gpu_type_ = config.value("gpu_type", "nvidia");
}

double GpuUsageWidget::read_nvidia_usage() {
    try {
        FILE* pipe = popen("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
        if (!pipe) return -1.0;
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        if (!result.empty()) {
            return std::stod(result);
        }
    } catch (...) {}
    return -1.0;
}

double GpuUsageWidget::read_amd_usage() {
    try {
        std::ifstream file("/sys/class/drm/card0/device/gpu_busy_percent");
        if (file) {
            int usage;
            file >> usage;
            return static_cast<double>(usage);
        }
    } catch (...) {}
    return -1.0;
}

void GpuUsageWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        double usage = -1.0;
        if (gpu_type_ == "nvidia") {
            usage = read_nvidia_usage();
        } else if (gpu_type_ == "amd") {
            usage = read_amd_usage();
        }
        
        if (usage >= 0.0) {
            value_ = std::to_string(static_cast<int>(usage));
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "GpuUsage error: " << e.what() << "\n";
        value_ = "GPU ERR";
    }
    
    last_tick_ = now;
}

// GpuMemoryWidget
GpuMemoryWidget::GpuMemoryWidget(const json& config)
    : Widget(config), last_tick_(std::chrono::steady_clock::now() - interval_) {
    gpu_type_ = config.value("gpu_type", "nvidia");
    display_mode_ = config.value("display_mode", "percent");
}

GpuMemoryWidget::GpuMemInfo GpuMemoryWidget::read_gpu_memory() {
    GpuMemInfo info;
    try {
        if (gpu_type_ == "nvidia") {
            FILE* pipe = popen("nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits 2>/dev/null", "r");
            if (pipe) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    std::istringstream iss(buffer);
                    std::string used_str, total_str;
                    std::getline(iss, used_str, ',');
                    std::getline(iss, total_str);
                    
                    info.used = std::stoull(used_str) * 1024 * 1024;  // MB to bytes
                    info.total = std::stoull(total_str) * 1024 * 1024;
                    if (info.total > 0) {
                        info.percent = 100.0 * info.used / info.total;
                    }
                }
                pclose(pipe);
            }
        }
    } catch (...) {}
    return info;
}

void GpuMemoryWidget::tick() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick_) < interval_) return;
    
    try {
        auto info = read_gpu_memory();
        std::ostringstream oss;
        
        if (info.total > 0) {
            if (display_mode_ == "percent") {
                oss << std::fixed << std::setprecision(0) << info.percent << "%";
            } else if (display_mode_ == "used") {
                double used_gb = info.used / (1024.0 * 1024.0 * 1024.0);
                oss << std::fixed << std::setprecision(1) << used_gb << " GB";
            } else if (display_mode_ == "total") {
                double total_gb = info.total / (1024.0 * 1024.0 * 1024.0);
                oss << std::fixed << std::setprecision(1) << total_gb << " GB";
            }
            value_ = oss.str();
        } else {
            value_ = "N/A";
        }
    } catch (const std::exception& e) {
        std::cerr << "GpuMemory error: " << e.what() << "\n";
        value_ = "GMEM ERR";
    }
    
    last_tick_ = now;
}

