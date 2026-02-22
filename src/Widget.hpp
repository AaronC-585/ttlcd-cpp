// src/Widget.hpp
#pragma once

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class Layout;

enum class WidgetType { Text, Bar };
enum class BarOrientation { Horizontal, Vertical };

class Widget {
protected:
    json config_;
    int x_ = 0, y_ = 0;
    int font_size_ = 14;
    cv::Scalar font_color_ = cv::Scalar(255, 255, 255);
    std::string value_;

    WidgetType type_ = WidgetType::Text;
    BarOrientation bar_ori_ = BarOrientation::Horizontal;
    std::string bar_direction_ = "right";
    int bar_width_ = 100;
    int bar_height_ = 20;
    int bar_scale_ = 100;
    cv::Scalar bar_fill_ = cv::Scalar(0, 255, 255);
    cv::Scalar bar_outline_ = cv::Scalar(0, 0, 255);

public:
    Widget(const json& config);
    virtual ~Widget() = default;
    virtual void tick() = 0;
    void draw(cv::Mat& image, Layout* layout = nullptr);

    void set_position(int x, int y) { x_ = x; y_ = y; }
    void set_font_size(int s) { font_size_ = s; }
    void set_font_color(cv::Scalar c) { font_color_ = c; }
    void set_type(WidgetType t) { type_ = t; }
    void set_bar_orientation(BarOrientation o) { bar_ori_ = o; }
    void set_bar_direction(const std::string& d) { bar_direction_ = d; }
    void set_bar_size(int w, int h) { bar_width_ = w; bar_height_ = h; }
    void set_bar_scale(int s) { bar_scale_ = s; }  // Now valid
    void set_bar_colors(cv::Scalar fill, cv::Scalar outline) {
        bar_fill_ = fill;
        bar_outline_ = outline;
    }
};

class DateWidget : public Widget {
public:
    DateWidget(const json& c) : Widget(c) {}
    void tick() override;
};

class TimeWidget : public Widget {
public:
    TimeWidget(const json& c) : Widget(c) {}
    void tick() override;
};

class LoadAverageWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
public:
    LoadAverageWidget(const json& c);
    void tick() override;
};

class CpuUtilizationWidget : public Widget {
private:
    struct CpuTimes {
        unsigned long long user{}, nice{}, system{}, idle{}, iowait{}, irq{}, softirq{}, steal{};
        unsigned long long total() const { return user + nice + system + idle + iowait + irq + softirq + steal; }
        unsigned long long idle_time() const { return idle + iowait; }
    };
    CpuTimes prev_;
    bool initialized_ = false;
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    CpuTimes read_cpu_times();
public:
    CpuUtilizationWidget(const json& c);
    void tick() override;
};

class RamUtilizationBarWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
public:
    RamUtilizationBarWidget(const json& c);
    void tick() override;
};

class NetworkSpeedWidget : public Widget {
private:
    struct NetStats { uint64_t rx = 0, tx = 0; };
    NetStats prev_;
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    bool initialized_ = false;
    NetStats read_net_stats();
public:
    NetworkSpeedWidget(const json& c);
    void tick() override;
};

class DynamicTextWidget : public Widget {
private:
    std::string format_string_;
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};

    std::string get_dynamic_value();
    std::string get_current_time();
public:
    DynamicTextWidget(const json& c);
    void tick() override;
};
class TextWidget : public Widget {
private:
    std::string static_text_;
public:
    TextWidget(const json& c);
    void tick() override;  // No update needed for static text
};

class LineWidget : public Widget {
private:
    int x2_ = 0;
    int y2_ = 0;
    int thickness_ = 1;
    cv::Scalar line_color_ = cv::Scalar(255, 255, 255);
    std::string orientation_ = "horizontal";  // "horizontal" or "vertical"
    int length_ = 100;
public:
    LineWidget(const json& c);
    void tick() override;
    void draw(cv::Mat& image, Layout* layout = nullptr);
};

// Temperature Sensors
class CpuTempWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string sensor_path_;
    double read_temperature();
public:
    CpuTempWidget(const json& c);
    void tick() override;
};

class GpuTempWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string gpu_type_;  // "nvidia" or "amd"
    double read_nvidia_temp();
    double read_amd_temp();
public:
    GpuTempWidget(const json& c);
    void tick() override;
};

// Disk Usage Widgets
class DiskUsageWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{5};
    std::string mount_point_;
    struct DiskStats {
        uint64_t total = 0;
        uint64_t used = 0;
        uint64_t available = 0;
        double percent = 0.0;
    };
    DiskStats read_disk_usage();
public:
    DiskUsageWidget(const json& c);
    void tick() override;
};

class DiskIOWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    std::string device_;
    bool initialized_ = false;
    struct IOStats {
        uint64_t read_bytes = 0;
        uint64_t write_bytes = 0;
    };
    IOStats prev_;
    IOStats read_disk_io();
public:
    DiskIOWidget(const json& c);
    void tick() override;
};

// Per-Core CPU Widget
class CpuCoreWidget : public Widget {
private:
    struct CoreTimes {
        unsigned long long user{}, nice{}, system{}, idle{}, iowait{}, irq{}, softirq{}, steal{};
        unsigned long long total() const { return user + nice + system + idle + iowait + irq + softirq + steal; }
        unsigned long long idle_time() const { return idle + iowait; }
    };
    std::vector<CoreTimes> prev_cores_;
    bool initialized_ = false;
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    int core_id_;  // -1 for all cores, or specific core number
    std::vector<CoreTimes> read_core_times();
public:
    CpuCoreWidget(const json& c);
    void tick() override;
};

// CPU Frequency Widget
class CpuFreqWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    int core_id_;  // -1 for average, or specific core
    double read_cpu_freq();
public:
    CpuFreqWidget(const json& c);
    void tick() override;
};

// Memory Details Widget
class MemoryDetailsWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string display_mode_;  // "total", "used", "free", "available", "cached", "swap"
    struct MemInfo {
        uint64_t total = 0;
        uint64_t free = 0;
        uint64_t available = 0;
        uint64_t buffers = 0;
        uint64_t cached = 0;
        uint64_t swap_total = 0;
        uint64_t swap_free = 0;
    };
    MemInfo read_meminfo();
public:
    MemoryDetailsWidget(const json& c);
    void tick() override;
};

// Swap Usage Widget
class SwapUsageWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
public:
    SwapUsageWidget(const json& c);
    void tick() override;
};

// Process Count Widget
class ProcessCountWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    int count_processes();
public:
    ProcessCountWidget(const json& c);
    void tick() override;
};

// Uptime Widget
class UptimeWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{10};
    std::string format_uptime(long seconds);
public:
    UptimeWidget(const json& c);
    void tick() override;
};

// Fan Speed Widget
class FanSpeedWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string sensor_path_;
    int read_fan_speed();
public:
    FanSpeedWidget(const json& c);
    void tick() override;
};

// Battery Widget (for laptops)
class BatteryWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{5};
    std::string battery_path_;
    struct BatteryInfo {
        int percent = 0;
        std::string status = "Unknown";
        bool charging = false;
    };
    BatteryInfo read_battery();
public:
    BatteryWidget(const json& c);
    void tick() override;
};

// Network Interface Details Widget
class NetworkInterfaceWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string interface_;
    std::string display_mode_;  // "ip", "status", "packets", "errors"
    std::string read_interface_info();
public:
    NetworkInterfaceWidget(const json& c);
    void tick() override;
};

// GPU Usage Widget (NVIDIA)
class GpuUsageWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{1};
    std::string gpu_type_;
    double read_nvidia_usage();
    double read_amd_usage();
public:
    GpuUsageWidget(const json& c);
    void tick() override;
};

// GPU Memory Widget
class GpuMemoryWidget : public Widget {
private:
    std::chrono::steady_clock::time_point last_tick_;
    std::chrono::seconds interval_{2};
    std::string gpu_type_;
    std::string display_mode_;  // "used", "total", "percent"
    struct GpuMemInfo {
        uint64_t total = 0;
        uint64_t used = 0;
        double percent = 0.0;
    };
    GpuMemInfo read_gpu_memory();
public:
    GpuMemoryWidget(const json& c);
    void tick() override;
};
