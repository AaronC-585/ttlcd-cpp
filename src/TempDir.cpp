// src/TempDir.cpp
#include "TempDir.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;

TempDir::TempDir() {
    auto tmp_base = fs::temp_directory_path();

    // High-quality randomness for uniqueness
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    const int max_attempts = 100;  // More than enough

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        uint64_t r1 = dist(gen);
        uint64_t r2 = dist(gen);

        std::ostringstream oss;
        oss << "ttlcd-" << std::hex << std::setw(16) << std::setfill('0') << r1
            << std::setw(16) << std::setfill('0') << r2;

        dir_ = tmp_base / oss.str();

        std::error_code ec;
        if (fs::create_directory(dir_, ec)) {
            return;  // Success!
        }

        if (ec.value() != static_cast<int>(std::errc::file_exists)) {
            throw std::runtime_error("Failed to create temp directory: " + ec.message());
        }
        // Collision — very rare, try again
    }

    throw std::runtime_error("Failed to create unique temporary directory after multiple attempts");
}

TempDir::~TempDir() {
    try {
        if (fs::exists(dir_)) {
            fs::remove_all(dir_);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not clean up temp directory '" << dir_ << "': "
                  << e.what() << std::endl;
    }
}

std::string TempDir::path() const {
    return dir_.string();
}
