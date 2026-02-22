// src/LCDController.hpp
#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <libusb-1.0/libusb.h>
#include <nlohmann/json.hpp>
#include "Layout.hpp"

using json = nlohmann::json;

class LCDController {
private:
    libusb_context* usb_ctx_ = nullptr;
    libusb_device_handle* dev_handle_ = nullptr;

    int vendor_id_;
    int product_id_;

    std::unique_ptr<Layout> layout_;

    uint8_t ep_write_ = 0x01;
    uint8_t ep_main_  = 0x03;

    // Configurable timeouts (milliseconds)
    int usb_timeout_ms_ = 30000;
    int packet_delay_ms_ = 1000;
    int update_interval_sec_ = 1;
    
    // Ping-pong system
    bool enable_ping_ = true;
    int ping_interval_sec_ = 30;
    std::chrono::steady_clock::time_point last_ping_;

    void initialize_device();
    std::string libusb_error_string(int error_code);
    void send_packet(const uint8_t* data, size_t length, uint8_t endpoint);
    void send_image(const std::string& image_path);
    void send_ping();
    bool should_send_ping();

public:
    explicit LCDController(const std::string& config_path);
    ~LCDController();

    void update_and_send();
    int get_update_interval() const { return update_interval_sec_; }
};
