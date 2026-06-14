// src/Layout.hpp
#pragma once

#include <cmath>
#include <vector>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/freetype.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include "Widget.hpp"

using json = nlohmann::json;

// Thermaltake Tower 200 3.9" panel: 480×128 native resolution
inline constexpr int LCD_WIDTH = 480;
inline constexpr int LCD_HEIGHT = 128;
inline constexpr double LCD_DIAGONAL_IN = 3.9;
inline constexpr double LCD_MAX_DPI =
    std::hypot(static_cast<double>(LCD_WIDTH), static_cast<double>(LCD_HEIGHT)) / LCD_DIAGONAL_IN;
inline constexpr double REFERENCE_DPI = 96.0;
// Layout/widget coordinates are defined at this JPEG DPI tier (reference ttlcd.py uses 300).
inline constexpr int DESIGN_JPEG_DPI = 300;
inline constexpr int DEFAULT_JPEG_DPI = 150;

class Layout {
protected:
    json config_;
    std::string background_path_;
    cv::Mat background_;
    std::vector<std::unique_ptr<Widget>> widgets_;

public:
    explicit Layout(const json& config);
    virtual ~Layout() = default;

    virtual bool setup() = 0;
    void warmup();
    void refresh_widgets();
    std::vector<uint8_t> render_jpeg();
    std::string render(const std::string& output_path);

    cv::Ptr<cv::freetype::FreeType2> get_ft2() const { return ft2_; }
    cv::Ptr<cv::freetype::FreeType2> ft2_;
    std::vector<unsigned char> font_data_;

    double get_font_scale_factor() const;
    int scale_font_size(int base_size) const;
    int jpeg_dpi() const;
    int render_width() const;
    int render_height() const;
    int scale_design_x(int x) const;
    int scale_design_y(int y) const;
    int scale_design_size(int px) const;

protected:
    void load_background();
    void add_widget(std::unique_ptr<Widget> widget);
    cv::Mat compose_frame();
    std::vector<uint8_t> encode_jpeg(const cv::Mat& image) const;
};

class NodeLayout : public Layout {
public:
    explicit NodeLayout(const json& config) : Layout(config) {}
    bool setup() override;
};
