# =============================================================================
# TTLCD-CPP Makefile
# Target: Linux only, built with GCC
# =============================================================================

# Compiler and flags
CXX      := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic
CXXFLAGS += -Wno-deprecated-enum-enum-conversion
CPPFLAGS := -Iinclude $(shell pkg-config --cflags libusb-1.0 opencv4)
LDFLAGS  := $(shell pkg-config --libs libusb-1.0 opencv4)
LDLIBS   := -lpthread

# Project structure
SOURCES  := main.cpp \
            src/Widget.cpp \
            src/Layout.cpp \
            src/LCDController.cpp \
            src/TempDir.cpp

OBJECTS  := $(SOURCES:.cpp=.o)
TARGET   := ttlcd

# Version handling
VERSION_FILE := src/Version.hpp
VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_PATCH_FILE := .build_patch  # File to store patch number

# Read current patch, increment it
CURRENT_PATCH := $(shell if [ -f $(VERSION_PATCH_FILE) ]; then cat $(VERSION_PATCH_FILE); else echo 0; fi)
NEXT_PATCH := $(shell echo $$(($(CURRENT_PATCH) + 1)))

# Default target
all: version $(TARGET)

# Generate Version.hpp with build date and incremented version
version:
	@echo "Generating $(VERSION_FILE) (v$(VERSION_MAJOR).$(VERSION_MINOR).$(NEXT_PATCH))"
	@echo "#pragma once" > $(VERSION_FILE)
	@echo "" >> $(VERSION_FILE)
	@echo "#include <string>" >> $(VERSION_FILE)
	@echo "" >> $(VERSION_FILE)
	@echo "namespace Version {" >> $(VERSION_FILE)
	@echo "    constexpr const char* BUILD_DATE = \"$(shell date '+%B %d, %Y')\";" >> $(VERSION_FILE)
	@echo "    constexpr int MAJOR = $(VERSION_MAJOR);" >> $(VERSION_FILE)
	@echo "    constexpr int MINOR = $(VERSION_MINOR);" >> $(VERSION_FILE)
	@echo "    constexpr int PATCH = $(NEXT_PATCH);" >> $(VERSION_FILE)
	@echo "" >> $(VERSION_FILE)
	@echo "    inline std::string get_version() {" >> $(VERSION_FILE)
	@echo "        return std::to_string(MAJOR) + \".\" + std::to_string(MINOR) + \".\" + std::to_string(PATCH);" >> $(VERSION_FILE)
	@echo "    }" >> $(VERSION_FILE)
	@echo "" >> $(VERSION_FILE)
	@echo "    inline std::string get_full_info() {" >> $(VERSION_FILE)
	@echo "        return \"ttlcd-cpp v\" + get_version() + \" (built \" + BUILD_DATE + \")\";" >> $(VERSION_FILE)
	@echo "    }" >> $(VERSION_FILE)
	@echo "}" >> $(VERSION_FILE)
	@echo $(NEXT_PATCH) > $(VERSION_PATCH_FILE)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

# Compile source files
%.o: %.cpp $(VERSION_FILE)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Clean
clean:
	$(RM) $(OBJECTS) $(SOURCES:.cpp=.d) $(TARGET) $(VERSION_FILE) $(VERSION_PATCH_FILE)

.PHONY: all clean version
