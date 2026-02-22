// src/LCDController.cpp
#include "LCDController.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

constexpr int IMAGE_PACKET_SIZE = 1020;
constexpr int IMAGE_CMD_SIZE = 4;

LCDController::LCDController(const std::string& config_path) {
    // Load JSON config
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }

    json config;
    file >> config;

    vendor_id_  = config["idVendor"].get<int>();
    product_id_ = config["idProduct"].get<int>();

    // Load timeout configuration with defaults
    usb_timeout_ms_ = config.value("usb_timeout_ms", 30000);
    packet_delay_ms_ = config.value("packet_delay_ms", 1000);
    update_interval_sec_ = config.value("update_interval_sec", 1);
    
    // Load ping configuration
    enable_ping_ = config.value("enable_ping", true);
    ping_interval_sec_ = config.value("ping_interval_sec", 30);
    last_ping_ = std::chrono::steady_clock::now();

    std::cout << "Configuration loaded:\n"
              << "  USB timeout: " << usb_timeout_ms_ << "ms\n"
              << "  Packet delay: " << packet_delay_ms_ << "ms\n"
              << "  Update interval: " << update_interval_sec_ << "s\n"
              << "  Ping enabled: " << (enable_ping_ ? "yes" : "no") << "\n"
              << "  Ping interval: " << ping_interval_sec_ << "s\n";

    // Initialize libusb
    int r = libusb_init(&usb_ctx_);
    if (r < 0) {
        throw std::runtime_error("Failed to initialize libusb: " + libusb_error_string(r));
    }

    // Open device
    dev_handle_ = libusb_open_device_with_vid_pid(usb_ctx_, vendor_id_, product_id_);
    if (!dev_handle_) {
        throw std::runtime_error("Thermaltake LCD device not found. Check USB connection and permissions.");
    }

    // Detach kernel driver if active
    if (libusb_kernel_driver_active(dev_handle_, 0) == 1) {
        libusb_detach_kernel_driver(dev_handle_, 0);
    }
    if (libusb_kernel_driver_active(dev_handle_, 1) == 1) {
        libusb_detach_kernel_driver(dev_handle_, 1);
    }

    // Claim interfaces
    libusb_claim_interface(dev_handle_, 0);
    libusb_claim_interface(dev_handle_, 1);

    std::cout << "Connected to Thermaltake LCD (VID:0x" << std::hex << vendor_id_
              << " PID:0x" << product_id_ << ")" << std::dec << std::endl;

    // Critical: Perform device initialization sequence
    initialize_device();

    // Create layout
    std::string use = config.value("use", "NODE");
    if (use == "NODE") {
        layout_ = std::make_unique<NodeLayout>(config);
    } else {
        throw std::runtime_error("Unknown layout type: " + use);
    }

    if (!layout_->setup()) {
        throw std::runtime_error("Failed to set up layout");
    }

    std::cout << "LCDController initialized successfully.\n";
}

LCDController::~LCDController() {
    if (dev_handle_) {
        libusb_release_interface(dev_handle_, 0);
        libusb_release_interface(dev_handle_, 1);
        libusb_close(dev_handle_);
    }
    if (usb_ctx_) {
        libusb_exit(usb_ctx_);
    }
}


void LCDController::update_and_send() {
    try {
        // Check if we need to send a ping to keep the device alive
        if (should_send_ping()) {
            send_ping();
        }
        
        // Use a fixed temporary path in /tmp (safe and always writable)
        const std::string temp_path = "/tmp/ttlcd_current.jpg";

        // Render directly to the temp file
        std::string image_path = layout_->render(temp_path);

        // Send the rendered image
        send_image(image_path);

        std::cout << "Dashboard updated on LCD\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to update display: " << e.what() << std::endl;
    }
}

void LCDController::send_image(const std::string& image_path) {
    std::ifstream file(image_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open rendered image: " + image_path);
    }

    std::vector<uint8_t> jpeg_data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();

    size_t total_size = jpeg_data.size();
    size_t iterations = (total_size + IMAGE_PACKET_SIZE - 1) / IMAGE_PACKET_SIZE;

    std::cout << "Sending image: " << total_size << " bytes in " << iterations << " packets\n";

    size_t offset = 0;
    int pkt_index = 0;

    while (offset < total_size) {
        std::vector<uint8_t> packet;

        if (pkt_index == 0) {
            packet.push_back(0x08);
            packet.push_back(static_cast<uint8_t>(iterations & 0xFF));
            packet.push_back(0x00);
            packet.push_back(0x80);
        } else {
            packet.push_back(0x08);
            packet.push_back(static_cast<uint8_t>(pkt_index & 0xFF));
            packet.push_back(0x00);
            packet.push_back(0x00);
        }

        size_t remaining = total_size - offset;
        size_t chunk = std::min<size_t>(IMAGE_PACKET_SIZE, remaining);

        packet.insert(packet.end(), jpeg_data.begin() + offset, jpeg_data.begin() + offset + chunk);

        while (packet.size() < IMAGE_PACKET_SIZE + IMAGE_CMD_SIZE) {
            packet.push_back(0x00);
        }

        send_packet(packet.data(), packet.size(), ep_main_);

        offset += chunk;
        pkt_index++;
    }

    std::cout << "Image sent successfully.\n";
}

void LCDController::initialize_device() {
    uint8_t ep_out_init = 0x01;
    const int padding = 436;

    auto build_packet = [](int padding_len, const std::vector<uint8_t>& args) -> std::vector<uint8_t> {
        std::vector<uint8_t> packet = args;
        if (padding_len > 0) {
            packet.resize(packet.size() + padding_len, 0x00);
        }
        return packet;
    };

    // Exact sequence from original ttlcd.py
    send_packet(build_packet(padding, {0x85, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);
    send_packet(build_packet(padding, {0x87, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);
    send_packet(build_packet(padding, {0x85, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);
    send_packet(build_packet(padding, {0x87, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);
    send_packet(build_packet(padding, {0x84, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);
    send_packet(build_packet(padding, {0x81, 0x01, 0x00, 0x80}).data(), 440, ep_out_init);

    libusb_clear_halt(dev_handle_, ep_main_ | LIBUSB_ENDPOINT_OUT);

    std::cout << "Device initialization sequence completed.\n";
}

std::string LCDController::libusb_error_string(int error_code) {
    switch (error_code) {
        case LIBUSB_SUCCESS:                return "Success";
        case LIBUSB_ERROR_IO:               return "Input/output error";
        case LIBUSB_ERROR_INVALID_PARAM:    return "Invalid parameter";
        case LIBUSB_ERROR_ACCESS:           return "Access denied";
        case LIBUSB_ERROR_NO_DEVICE:        return "No device (disconnected)";
        case LIBUSB_ERROR_NOT_FOUND:        return "Entity not found";
        case LIBUSB_ERROR_BUSY:             return "Resource busy";
        case LIBUSB_ERROR_TIMEOUT:          return "Operation timed out";
        case LIBUSB_ERROR_OVERFLOW:         return "Overflow";
        case LIBUSB_ERROR_PIPE:             return "Pipe error (endpoint halted)";
        case LIBUSB_ERROR_INTERRUPTED:      return "Interrupted";
        case LIBUSB_ERROR_NO_MEM:           return "Insufficient memory";
        case LIBUSB_ERROR_NOT_SUPPORTED:    return "Not supported";
        case LIBUSB_ERROR_OTHER:            return "Other error";
        default:                            return "Unknown error (" + std::to_string(error_code) + ")";
    }
}
void LCDController::send_packet(const uint8_t* data, size_t length, uint8_t endpoint) {
    int transferred = 0;
    int r = libusb_bulk_transfer(dev_handle_, endpoint | LIBUSB_ENDPOINT_OUT,
                                 const_cast<uint8_t*>(data), static_cast<int>(length),
                                 &transferred, usb_timeout_ms_);

    if (r != LIBUSB_SUCCESS) {
        throw std::runtime_error("USB transfer failed: " + libusb_error_string(r));
    }
    if (transferred != static_cast<int>(length)) {
        throw std::runtime_error("Incomplete transfer: sent " + std::to_string(transferred) +
                                 " of " + std::to_string(length) + " bytes");
    }

    // Use configurable delay between packets
    std::this_thread::sleep_for(std::chrono::milliseconds(packet_delay_ms_));
}

bool LCDController::should_send_ping() {
    if (!enable_ping_) return false;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_ping_).count();
    
    return elapsed >= ping_interval_sec_;
}

void LCDController::send_ping() {
    if (!enable_ping_) return;
    
    try {
        // Send a simple keep-alive packet (0x85 command similar to init sequence)
        uint8_t ping_packet[440] = {0x85, 0x01, 0x00, 0x80};
        // Rest is zeros (padding)
        
        send_packet(ping_packet, 440, ep_write_);
        
        last_ping_ = std::chrono::steady_clock::now();
        std::cout << "Ping sent to keep device connection alive\n";
    } catch (const std::exception& e) {
        std::cerr << "Warning: Ping failed: " << e.what() << std::endl;
        // Don't throw - just log the warning
    }
}
