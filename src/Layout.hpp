// src/Layout.hpp
#pragma once

#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/freetype.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include "Widget.hpp"

using json = nlohmann::json;

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
    std::string render(const std::string& output_path);

    cv::Ptr<cv::freetype::FreeType2> get_ft2() const { return ft2_; }
    cv::Ptr<cv::freetype::FreeType2> ft2_;

protected:
    void load_background();
    void add_widget(std::unique_ptr<Widget> widget);
};

class NodeLayout : public Layout {
public:
    explicit NodeLayout(const json& config) : Layout(config) {}
    bool setup() override;
};
