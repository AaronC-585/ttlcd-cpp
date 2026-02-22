// src/TempDir.hpp
#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class TempDir {
public:
    TempDir();
    ~TempDir();
    std::string path() const;

private:
    fs::path dir_;
};
