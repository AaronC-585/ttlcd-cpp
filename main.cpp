// main.cpp - Single live TTLCD application (render + send in loop)
#include "src/Layout.hpp"
#include "src/LCDController.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "src/Version.hpp"

namespace fs = std::filesystem;

std::string get_default_config_path(const std::string& executable_path) {
    // Get directory containing the executable
    fs::path exe_path(executable_path);
    fs::path exe_dir = exe_path.parent_path();
    
    // If parent_path is empty, we're running from current directory
    if (exe_dir.empty()) {
        exe_dir = fs::current_path();
    }
    
    // Look for config.json in the same directory as executable
    fs::path config_path = exe_dir / "config.json";
    
    return config_path.string();
}

int main(int argc, char** argv) {
    std::string config_path;
    
    // Determine config path
    if (argc == 2) {
        config_path = argv[1];
    } else if (argc == 1) {
        // Use default config.json in same directory as executable
        config_path = get_default_config_path(argv[0]);
        std::cout << "No config file specified, using default: " << config_path << "\n";
    } else {
        std::cerr << "Usage: " << argv[0] << " [config.json]\n";
        std::cerr << "If no config file is specified, will look for config.json in the same directory as the executable.\n";
        return 1;
    }

    // Check if config file exists
    if (!fs::exists(config_path)) {
        std::cerr << "Error: Config file not found: " << config_path << "\n";
        std::cerr << "Please provide a valid config.json file.\n";
        return 1;
    }

    try {
        LCDController lcd(config_path);

        std::cout << "TTLCD running... Press Ctrl+C to stop.\n";
        std::cout << Version::get_full_info() << std::endl;

        // Use the configurable update interval from config
        int update_interval = lcd.get_update_interval();
        std::cout << "Display will update every " << update_interval << " second(s)\n";

        while (true) {
            lcd.update_and_send();  // Renders in memory + sends via USB
            std::this_thread::sleep_for(std::chrono::seconds(update_interval));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
