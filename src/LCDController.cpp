// src/LCDController.cpp
#include "LCDController.hpp"
#include "TempUnits.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <unistd.h>

using json = nlohmann::json;

constexpr int IMAGE_PACKET_SIZE = 1020;
constexpr int IMAGE_CMD_SIZE = 4;
constexpr int INIT_PADDING = 436;

LCDController::LCDController(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }

    json config;
    file >> config;

    TempUnits::configure(config);

    vendor_id_  = config.value("idVendor", DEFAULT_VENDOR_ID);
    product_id_ = config.value("idProduct", DEFAULT_PRODUCT_ID);

    usb_timeout_ms_ = config.value("usb_timeout_ms", 30000);
    packet_delay_ms_ = config.value("packet_delay_ms", 0);
    ping_packet_delay_ms_ = config.value("ping_packet_delay_ms", 0);
    loop_interval_ms_ = config.value("loop_interval_ms", 1000);
    enable_dashboard_ = config.value("enable_dashboard", true);
    dashboard_interval_sec_ = config.value("dashboard_interval_sec",
        config.value("update_interval_sec", 10));

    enable_ping_ = config.value("enable_ping", true);
    ping_interval_sec_ = config.value("ping_interval_sec", 2);

    if (config.value("save_jpeg", false)) {
        save_jpeg_path_ = (std::filesystem::temp_directory_path() /
            ("ttlcd_" + std::to_string(getuid()) + "_current.jpg")).string();
    }

    const auto now = std::chrono::steady_clock::now();
    last_ping_ = now;
    last_display_update_ = std::chrono::steady_clock::time_point{};

    std::cout << "Configuration loaded:\n"
              << "  USB device: " << std::hex << std::showbase
              << vendor_id_ << ":" << product_id_ << std::dec << std::noshowbase << "\n"
              << "  USB timeout: " << usb_timeout_ms_ << "ms\n"
              << "  Image packet delay: " << packet_delay_ms_ << "ms\n"
              << "  Ping packet delay: " << ping_packet_delay_ms_ << "ms\n"
              << "  Loop interval: " << loop_interval_ms_ << "ms\n"
              << "  Dashboard enabled: " << (enable_dashboard_ ? "yes" : "no") << "\n"
              << "  Dashboard interval: " << dashboard_interval_sec_ << "s\n"
              << "  Ping enabled: " << (enable_ping_ ? "yes" : "no") << "\n"
              << "  Keep-alive interval: " << ping_interval_sec_ << "s\n";

    int r = libusb_init(&usb_ctx_);
    if (r < 0) {
        throw std::runtime_error("Failed to initialize libusb: " + libusb_error_string(r));
    }

    std::string use = config.value("use", "NODE");
    if (use == "NODE") {
        layout_ = std::make_unique<NodeLayout>(config);
    } else {
        throw std::runtime_error("Unknown layout type: " + use);
    }

    if (!layout_->setup()) {
        throw std::runtime_error("Failed to set up layout");
    }

    layout_->warmup();

    running_ = true;
    start_keepalive_thread();

    if (try_connect()) {
        std::cout << "LCDController initialized successfully.\n";
    } else {
        std::cout << "LCD device not found — waiting for connection...\n";
        device_wait_logged_ = true;
    }
}

LCDController::~LCDController() {
    stop_keepalive_thread();
    disconnect();
    if (usb_ctx_) {
        libusb_exit(usb_ctx_);
    }
}

bool LCDController::is_device_present() {
    libusb_device** list = nullptr;
    const ssize_t count = libusb_get_device_list(usb_ctx_, &list);
    if (count < 0) {
        return false;
    }

    bool found = false;
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor == static_cast<uint16_t>(vendor_id_) &&
            desc.idProduct == static_cast<uint16_t>(product_id_)) {
            found = true;
            break;
        }
    }

    libusb_free_device_list(list, 1);
    return found;
}

bool LCDController::try_connect() {
    if (connected_.load()) {
        return true;
    }
    if (!is_device_present()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(usb_mutex_);
    if (connected_.load()) {
        return true;
    }

    dev_handle_ = libusb_open_device_with_vid_pid(usb_ctx_, vendor_id_, product_id_);
    if (!dev_handle_) {
        return false;
    }

    try {
        if (libusb_kernel_driver_active(dev_handle_, 0) == 1) {
            libusb_detach_kernel_driver(dev_handle_, 0);
        }
        if (libusb_kernel_driver_active(dev_handle_, 1) == 1) {
            libusb_detach_kernel_driver(dev_handle_, 1);
        }

        libusb_set_configuration(dev_handle_, 1);
        libusb_claim_interface(dev_handle_, 0);
        libusb_claim_interface(dev_handle_, 1);

        std::cout << "Connected to Thermaltake LCD (VID:0x" << std::hex << vendor_id_
                  << " PID:0x" << product_id_ << ")" << std::dec << std::endl;

        initialize_device_unlocked();

        connected_ = true;
        first_upload_pending_ = true;
        last_jpeg_.clear();
        last_display_update_ = std::chrono::steady_clock::time_point{};
        last_ping_ = std::chrono::steady_clock::now();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect to LCD: " << e.what() << std::endl;
        disconnect_unlocked();
        return false;
    }
}

void LCDController::try_reconnect() {
    if (try_connect()) {
        if (device_wait_logged_) {
            std::cout << "LCD device connected.\n";
            device_wait_logged_ = false;
        }
    } else if (!device_wait_logged_) {
        std::cout << "LCD device not found — waiting for connection...\n";
        device_wait_logged_ = true;
    }
}

void LCDController::disconnect_unlocked() {
    connected_ = false;
    uploading_ = false;
    first_upload_pending_ = true;

    if (!dev_handle_) {
        return;
    }

    libusb_release_interface(dev_handle_, 0);
    libusb_release_interface(dev_handle_, 1);
    libusb_close(dev_handle_);
    dev_handle_ = nullptr;
}

void LCDController::disconnect() {
    std::lock_guard<std::mutex> lock(usb_mutex_);
    disconnect_unlocked();
}

void LCDController::tick() {
    if (!connected_.load()) {
        try_reconnect();
    }

    if (should_update_display()) {
        update_display();
    }
}

void LCDController::update_display() {
    try {
        const std::vector<uint8_t> jpeg = layout_->render_jpeg();

        if (!connected_.load()) {
            return;
        }

        if (!last_jpeg_.empty() && jpeg == last_jpeg_) {
            last_display_update_ = std::chrono::steady_clock::now();
            std::cout << "Dashboard unchanged, skipping upload\n";
            return;
        }

        last_jpeg_ = jpeg;

        std::cout << "Dynamic JPEG: " << jpeg.size() << " bytes ("
                  << LCD_WIDTH << "x" << LCD_HEIGHT << ")\n";

        if (!save_jpeg_path_.empty()) {
            std::ofstream out(save_jpeg_path_, std::ios::binary | std::ios::trunc);
            if (out) {
                out.write(reinterpret_cast<const char*>(jpeg.data()),
                          static_cast<std::streamsize>(jpeg.size()));
                std::cout << "Saved JPEG: " << save_jpeg_path_ << "\n";
            }
        }

        send_image(jpeg);
        last_display_update_ = std::chrono::steady_clock::now();
        std::cout << "Dashboard updated on LCD\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to update display: " << e.what() << std::endl;
        disconnect();
    }
}

void LCDController::prepare_before_upload_unlocked() {
    if (!first_upload_pending_) return;

    const uint8_t cmd[] = {0x12, 0x01, 0x00, 0x80, 0x64};
    send_padded_command(ep_write_, cmd, sizeof(cmd), 435, 0);
    first_upload_pending_ = false;
    std::cout << "Upload session prepared (0x12)\n";
}

void LCDController::prepare_before_upload() {
    std::lock_guard<std::mutex> lock(usb_mutex_);
    prepare_before_upload_unlocked();
}

void LCDController::confirm_after_upload_unlocked() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (try_recv_packet(ep_trigger_, 1, 2000, 16)) {
        std::cout << "Upload confirmed by device\n";
    } else {
        std::cerr << "Warning: Upload confirmation read timed out\n";
    }
}

void LCDController::confirm_after_upload() {
    std::lock_guard<std::mutex> lock(usb_mutex_);
    confirm_after_upload_unlocked();
}

void LCDController::send_image(const std::vector<uint8_t>& jpeg_data) {
    if (jpeg_data.empty()) {
        throw std::runtime_error("Rendered image is empty");
    }
    if (!connected_.load() || !dev_handle_) {
        throw std::runtime_error("LCD device not connected");
    }

    uploading_ = true;

    const size_t total_size = jpeg_data.size();
    const size_t iterations = (total_size + IMAGE_PACKET_SIZE - 1) / IMAGE_PACKET_SIZE;

    std::cout << "Sending image: " << total_size << " bytes in " << iterations << " packets\n";

    try {
        std::lock_guard<std::mutex> lock(usb_mutex_);
        if (!connected_.load() || !dev_handle_) {
            throw std::runtime_error("LCD device not connected");
        }

        prepare_before_upload_unlocked();

        size_t offset = 0;
        size_t index = 0;
        int pkt_index = 1;

        while (offset < total_size) {
            std::vector<uint8_t> packet;

            if (index == 0) {
                packet = {0x08, static_cast<uint8_t>(iterations & 0xFF), 0x00, 0x80};
            } else {
                packet = {0x08, static_cast<uint8_t>(pkt_index & 0xFF), 0x00, 0x00};
            }

            const size_t remaining = total_size - offset;
            const size_t chunk = std::min(static_cast<size_t>(IMAGE_PACKET_SIZE), remaining);
            packet.insert(packet.end(),
                          jpeg_data.begin() + static_cast<std::ptrdiff_t>(offset),
                          jpeg_data.begin() + static_cast<std::ptrdiff_t>(offset + chunk));

            const size_t target_size = IMAGE_PACKET_SIZE + IMAGE_CMD_SIZE;
            if (packet.size() < target_size) {
                packet.resize(target_size, 0x00);
            }

            send_packet(packet.data(), packet.size(), ep_main_, packet_delay_ms_);

            offset += chunk;
            if (index > 0) {
                ++pkt_index;
            }
            ++index;
        }

        confirm_after_upload_unlocked();
    } catch (...) {
        uploading_ = false;
        throw;
    }

    uploading_ = false;
    std::cout << "Image sent successfully.\n";
}

void LCDController::read_string_descriptors() {
    const uint8_t indices[] = {0x02, 0x03, 0x02, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};
    unsigned char buf[256];

    for (const uint8_t index : indices) {
        const int r = libusb_get_string_descriptor_ascii(dev_handle_, index, buf, sizeof(buf));
        if (r < 0) {
            std::cerr << "Warning: String descriptor read failed for index "
                      << static_cast<int>(index) << " (continuing)\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LCDController::initialize_device_unlocked() {
    read_string_descriptors();

    const uint8_t init_sequence[][4] = {
        {0x85, 0x01, 0x00, 0x80},
        {0x87, 0x01, 0x00, 0x80},
        {0x85, 0x01, 0x00, 0x80},
        {0x87, 0x01, 0x00, 0x80},
        {0x84, 0x01, 0x00, 0x80},
        {0x81, 0x01, 0x00, 0x80},
    };

    for (const auto& cmd : init_sequence) {
        send_padded_command(ep_write_, cmd, 4, INIT_PADDING, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!try_recv_packet(ep_read_, 1, 10000, 440)) {
            std::cerr << "Warning: Init handshake read timed out (continuing)\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    libusb_clear_halt(dev_handle_, ep_main_ | LIBUSB_ENDPOINT_OUT);
    std::cout << "Device initialization sequence completed.\n";
}

void LCDController::send_padded_command(uint8_t endpoint, const uint8_t* cmd, size_t cmd_len,
                                        int padding, int post_delay_ms) {
    std::vector<uint8_t> packet(cmd, cmd + cmd_len);
    if (padding > 0) {
        packet.resize(packet.size() + static_cast<size_t>(padding), 0x00);
    }
    send_packet(packet.data(), packet.size(), endpoint, post_delay_ms);
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

void LCDController::send_packet(const uint8_t* data, size_t length, uint8_t endpoint, int post_delay_ms) {
    int transferred = 0;
    int r = libusb_bulk_transfer(dev_handle_, endpoint | LIBUSB_ENDPOINT_OUT,
                                 const_cast<uint8_t*>(data), static_cast<int>(length),
                                 &transferred, usb_timeout_ms_);

    if (r != LIBUSB_SUCCESS) {
        if (r == LIBUSB_ERROR_NO_DEVICE || r == LIBUSB_ERROR_PIPE) {
            disconnect_unlocked();
        }
        throw std::runtime_error("USB transfer failed: " + libusb_error_string(r));
    }
    if (transferred != static_cast<int>(length)) {
        throw std::runtime_error("Incomplete transfer: sent " + std::to_string(transferred) +
                                 " of " + std::to_string(length) + " bytes");
    }

    if (post_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(post_delay_ms));
    }
}

void LCDController::recv_packet(uint8_t endpoint, size_t min_bytes, int timeout_ms, size_t buffer_size) {
    if (!try_recv_packet(endpoint, min_bytes, timeout_ms, buffer_size)) {
        throw std::runtime_error("USB receive timed out");
    }
}

bool LCDController::try_recv_packet(uint8_t endpoint, size_t min_bytes, int timeout_ms, size_t buffer_size) {
    if (buffer_size < min_bytes) {
        buffer_size = min_bytes;
    }

    std::vector<uint8_t> buffer(buffer_size);
    int transferred = 0;
    int r = libusb_bulk_transfer(dev_handle_, endpoint | LIBUSB_ENDPOINT_IN,
                                 buffer.data(), static_cast<int>(buffer_size),
                                 &transferred, timeout_ms);

    if (r == LIBUSB_ERROR_TIMEOUT) {
        return false;
    }

    // Device may send a full 440-byte frame when fewer bytes were expected.
    if (r == LIBUSB_ERROR_OVERFLOW) {
        return transferred >= static_cast<int>(min_bytes);
    }

    if (r != LIBUSB_SUCCESS) {
        if (r == LIBUSB_ERROR_NO_DEVICE || r == LIBUSB_ERROR_PIPE) {
            disconnect_unlocked();
        }
        throw std::runtime_error("USB receive failed: " + libusb_error_string(r));
    }

    return transferred >= static_cast<int>(min_bytes);
}

void LCDController::drain_in_endpoint(uint8_t endpoint) {
    std::vector<uint8_t> buffer(512);
    for (int attempt = 0; attempt < 8; ++attempt) {
        int transferred = 0;
        const int r = libusb_bulk_transfer(dev_handle_, endpoint | LIBUSB_ENDPOINT_IN,
                                           buffer.data(), static_cast<int>(buffer.size()),
                                           &transferred, 50);
        if (r == LIBUSB_ERROR_TIMEOUT || transferred == 0) {
            return;
        }
        if (r != LIBUSB_SUCCESS && r != LIBUSB_ERROR_OVERFLOW) {
            return;
        }
    }
}

void LCDController::clear_in_halt(uint8_t endpoint) {
    libusb_clear_halt(dev_handle_, endpoint | LIBUSB_ENDPOINT_IN);
}

bool LCDController::should_send_ping() const {
    if (!enable_ping_) return false;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_ping_).count();
    return elapsed >= ping_interval_sec_;
}

bool LCDController::should_update_display() const {
    if (!enable_dashboard_) return false;

    const auto now = std::chrono::steady_clock::now();
    if (last_display_update_.time_since_epoch().count() == 0) {
        return true;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_display_update_).count();
    return elapsed >= dashboard_interval_sec_;
}

void LCDController::send_keepalive_unlocked() {
    const uint8_t cmd[] = {0x82, 0x01, 0x00, 0x80};
    send_padded_command(ep_write_, cmd, sizeof(cmd), INIT_PADDING, ping_packet_delay_ms_);
    last_ping_ = std::chrono::steady_clock::now();
    std::cout << "Keep-alive sent (0x82)\n";
}

void LCDController::start_keepalive_thread() {
    keepalive_thread_ = std::thread([this]() { keepalive_loop(); });
}

void LCDController::stop_keepalive_thread() {
    running_ = false;
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
}

void LCDController::keepalive_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!connected_.load() || !enable_ping_ || uploading_.load() || !should_send_ping()) {
            continue;
        }

        try {
            std::lock_guard<std::mutex> lock(usb_mutex_);
            if (!connected_.load() || !dev_handle_) {
                continue;
            }
            send_keepalive_unlocked();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Keep-alive failed: " << e.what() << std::endl;
            disconnect_unlocked();
        }
    }
}
