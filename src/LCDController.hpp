// src/LCDController.hpp
#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <libusb-1.0/libusb.h>
#include <nlohmann/json.hpp>
#include "Layout.hpp"

using json = nlohmann::json;

// Default Thermaltake LCD USB ID (264a:233d)
inline constexpr int DEFAULT_VENDOR_ID  = 0x264a;
inline constexpr int DEFAULT_PRODUCT_ID = 0x233d;

class LCDController {
private:
    libusb_context* usb_ctx_ = nullptr;
    libusb_device_handle* dev_handle_ = nullptr;

    int vendor_id_;
    int product_id_;

    std::unique_ptr<Layout> layout_;

    // Interface 0: 0x01 OUT (440), 0x82 IN (440). Interface 1: 0x03 OUT (1024), 0x84 IN (16).
    uint8_t ep_write_   = 0x01;
    uint8_t ep_read_    = 0x82;
    uint8_t ep_main_    = 0x03;
    uint8_t ep_trigger_ = 0x84;

    int usb_timeout_ms_ = 30000;
    int packet_delay_ms_ = 0;
    int ping_packet_delay_ms_ = 0;
    int loop_interval_ms_ = 1000;
    int dashboard_interval_sec_ = 10;

    bool enable_dashboard_ = true;
    bool enable_ping_ = true;
    int ping_interval_sec_ = 3;
    bool first_upload_pending_ = true;
    std::atomic<bool> uploading_{false};
    std::vector<uint8_t> last_jpeg_;

    std::string save_jpeg_path_;

    std::mutex usb_mutex_;
    std::thread keepalive_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    bool device_wait_logged_ = false;

    std::chrono::steady_clock::time_point last_ping_;
    std::chrono::steady_clock::time_point last_display_update_;

    bool is_device_present();
    bool try_connect();
    void try_reconnect();
    void disconnect();
    void disconnect_unlocked();
    void read_string_descriptors();
    void start_keepalive_thread();
    void stop_keepalive_thread();
    void keepalive_loop();
    std::string libusb_error_string(int error_code);
    void send_packet(const uint8_t* data, size_t length, uint8_t endpoint, int post_delay_ms);
    void recv_packet(uint8_t endpoint, size_t min_bytes, int timeout_ms, size_t buffer_size = 440);
    bool try_recv_packet(uint8_t endpoint, size_t min_bytes, int timeout_ms, size_t buffer_size = 440);
    void drain_in_endpoint(uint8_t endpoint);
    void clear_in_halt(uint8_t endpoint);
    void send_padded_command(uint8_t endpoint, const uint8_t* cmd, size_t cmd_len, int padding, int post_delay_ms);
    void prepare_before_upload();
    void prepare_before_upload_unlocked();
    void confirm_after_upload();
    void confirm_after_upload_unlocked();
    void send_image(const std::vector<uint8_t>& jpeg_data);
    void initialize_device_unlocked();
    void send_keepalive_unlocked();
    void update_display();
    bool should_send_ping() const;
    bool should_update_display() const;

public:
    explicit LCDController(const std::string& config_path);
    ~LCDController();

    void tick();
    int get_loop_interval_ms() const { return loop_interval_ms_; }
    int get_dashboard_interval() const { return dashboard_interval_sec_; }
    int get_update_interval() const { return dashboard_interval_sec_; }
    int get_ping_interval() const { return ping_interval_sec_; }
};
