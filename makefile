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
VERSION_FILE       := src/Version.hpp
VERSION_MAJOR      := 1
VERSION_MINOR      := 0
VERSION_PATCH_FILE := .build_patch

CURRENT_PATCH := $(shell if [ -f $(VERSION_PATCH_FILE) ]; then cat $(VERSION_PATCH_FILE); else echo 0; fi)
NEXT_PATCH    := $(shell echo $$(($(CURRENT_PATCH) + 1)))
VERSION       := $(VERSION_MAJOR).$(VERSION_MINOR).$(NEXT_PATCH)

# Packaging output dirs
PKG_DEB_DIR  := packaging/debian
PKG_RPM_DIR  := packaging/rpm
PKG_ARCH_DIR := packaging/arch
GH_WORKFLOW  := .github/workflows

# Generated files
CONTROL   := $(PKG_DEB_DIR)/control
SPEC      := $(PKG_RPM_DIR)/ttlcd.spec
PKGBUILD  := $(PKG_ARCH_DIR)/PKGBUILD
CMAKE     := CMakeLists.txt
WORKFLOW  := $(GH_WORKFLOW)/release.yml

# =============================================================================
# Default target: build everything
# =============================================================================
all: version $(TARGET) packaging

packaging: $(CONTROL) $(SPEC) $(PKGBUILD) $(CMAKE) $(WORKFLOW)
	@echo "All packaging files generated."

# =============================================================================
# Version header
# =============================================================================
version:
	@echo "Generating $(VERSION_FILE) (v$(VERSION))"
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

# =============================================================================
# Build
# =============================================================================
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp $(VERSION_FILE)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# =============================================================================
# Debian control
# =============================================================================
$(CONTROL): | $(PKG_DEB_DIR)
	@echo "Generating $@"
	@printf 'Package: ttlcd\n' > $@
	@printf 'Version: $(VERSION)\n' >> $@
	@printf 'Section: utils\n' >> $@
	@printf 'Priority: optional\n' >> $@
	@printf 'Architecture: amd64\n' >> $@
	@printf 'Maintainer: Aaron C\n' >> $@
	@printf 'Depends: libusb-1.0-0, libopencv-core4.5 | libopencv-core4.6, libopencv-imgproc4.5 | libopencv-imgproc4.6, libopencv-imgcodecs4.5 | libopencv-imgcodecs4.6, libopencv-highgui4.5 | libopencv-highgui4.6\n' >> $@
	@printf 'Build-Depends: build-essential, pkg-config, libusb-1.0-0-dev, libopencv-dev, libfreetype6-dev\n' >> $@
	@printf 'Description: TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels\n' >> $@
	@printf ' A modern C++ utility for driving 3.9-inch USB LCD panels (480x128) with\n' >> $@
	@printf ' live system monitoring, custom TTF fonts, bar graphs, and JSON config.\n' >> $@

# =============================================================================
# RPM spec
# =============================================================================
$(SPEC): | $(PKG_RPM_DIR)
	@echo "Generating $@"
	@printf 'Name:           ttlcd\n' > $@
	@printf 'Version:        $(VERSION)\n' >> $@
	@printf 'Release:        1%%{?dist}\n' >> $@
	@printf 'Summary:        TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels\n' >> $@
	@printf 'License:        MIT\n' >> $@
	@printf 'URL:            https://github.com/AaronC-585/ttlcd\n' >> $@
	@printf 'Source0:        %%{name}-%%{version}.tar.gz\n' >> $@
	@printf '\n' >> $@
	@printf 'BuildRequires:  gcc-c++ make pkg-config libusb1-devel opencv-devel freetype-devel\n' >> $@
	@printf 'Requires:       libusb1 opencv freetype\n' >> $@
	@printf '\n' >> $@
	@printf '%%description\n' >> $@
	@printf 'A modern C++ utility for driving 3.9-inch USB LCD panels (480x128) with\n' >> $@
	@printf 'live system monitoring, custom TTF fonts via OpenCV FreeType, bar graphs,\n' >> $@
	@printf 'network speed, and full JSON configuration.\n' >> $@
	@printf '\n' >> $@
	@printf '%%prep\n%%autosetup\n' >> $@
	@printf '\n' >> $@
	@printf '%%build\n' >> $@
	@printf 'make -j$$(nproc)\n' >> $@
	@printf '\n' >> $@
	@printf '%%install\n' >> $@
	@printf 'install -Dm755 ttlcd %%{buildroot}%%{_bindir}/ttlcd\n' >> $@
	@printf '\n' >> $@
	@printf '%%files\n%%{_bindir}/ttlcd\n' >> $@
	@printf '\n' >> $@
	@printf '%%changelog\n' >> $@
	@printf '* $(shell date "+%%a %%b %%d %%Y") Your Name <you@example.com> - $(VERSION)-1\n' >> $@
	@printf '- Build $(VERSION)\n' >> $@

# =============================================================================
# Arch PKGBUILD
# =============================================================================
$(PKGBUILD): | $(PKG_ARCH_DIR)
	@echo "Generating $@"
	@printf '# Maintainer: Your Name <you@example.com>\n' > $@
	@printf 'pkgname=ttlcd\n' >> $@
	@printf 'pkgver=$(VERSION)\n' >> $@
	@printf 'pkgrel=1\n' >> $@
	@printf 'pkgdesc="TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels"\n' >> $@
	@printf "arch=('x86_64')\n" >> $@
	@printf 'url="https://github.com/AaronC-585/ttlcd"\n' >> $@
	@printf "license=('MIT')\n" >> $@
	@printf "depends=('libusb' 'opencv' 'freetype2')\n" >> $@
	@printf "makedepends=('gcc' 'pkg-config' 'make')\n" >> $@
	@printf 'source=("$$pkgname-$$pkgver.tar.gz::https://github.com/AaronC-585/ttlcd/archive/v$$pkgver.tar.gz")\n' >> $@
	@printf "sha256sums=('SKIP')\n" >> $@
	@printf '\n' >> $@
	@printf 'build() {\n' >> $@
	@printf '    cd "$$pkgname-$$pkgver"\n' >> $@
	@printf '    make -j$$(nproc)\n' >> $@
	@printf '}\n' >> $@
	@printf '\n' >> $@
	@printf 'package() {\n' >> $@
	@printf '    cd "$$pkgname-$$pkgver"\n' >> $@
	@printf '    install -Dm755 ttlcd "$$pkgdir/usr/bin/ttlcd"\n' >> $@
	@printf '}\n' >> $@

# =============================================================================
# CMakeLists.txt
# =============================================================================
$(CMAKE):
	@echo "Generating $@"
	@printf 'cmake_minimum_required(VERSION 3.20)\n' > $@
	@printf 'project(ttlcd VERSION $(VERSION))\n' >> $@
	@printf '\n' >> $@
	@printf 'set(CMAKE_CXX_STANDARD 20)\n' >> $@
	@printf 'set(CMAKE_CXX_STANDARD_REQUIRED ON)\n' >> $@
	@printf '\n' >> $@
	@printf 'find_package(PkgConfig REQUIRED)\n' >> $@
	@printf 'pkg_check_modules(LIBUSB REQUIRED libusb-1.0)\n' >> $@
	@printf 'pkg_check_modules(OPENCV REQUIRED opencv4)\n' >> $@
	@printf '\n' >> $@
	@printf 'add_executable(ttlcd\n' >> $@
	@printf '    main.cpp\n' >> $@
	@printf '    src/Widget.cpp\n' >> $@
	@printf '    src/Layout.cpp\n' >> $@
	@printf '    src/LCDController.cpp\n' >> $@
	@printf '    src/TempDir.cpp\n' >> $@
	@printf ')\n' >> $@
	@printf '\n' >> $@
	@printf 'target_include_directories(ttlcd PRIVATE\n' >> $@
	@printf '    include\n' >> $@
	@printf '    $${LIBUSB_INCLUDE_DIRS}\n' >> $@
	@printf '    $${OPENCV_INCLUDE_DIRS}\n' >> $@
	@printf ')\n' >> $@
	@printf '\n' >> $@
	@printf 'target_compile_options(ttlcd PRIVATE\n' >> $@
	@printf '    -O2 -Wall -Wextra -pedantic\n' >> $@
	@printf '    -Wno-deprecated-enum-enum-conversion\n' >> $@
	@printf '    $${LIBUSB_CFLAGS_OTHER}\n' >> $@
	@printf '    $${OPENCV_CFLAGS_OTHER}\n' >> $@
	@printf ')\n' >> $@
	@printf '\n' >> $@
	@printf 'target_link_libraries(ttlcd PRIVATE\n' >> $@
	@printf '    $${LIBUSB_LIBRARIES}\n' >> $@
	@printf '    $${OPENCV_LIBRARIES}\n' >> $@
	@printf '    pthread\n' >> $@
	@printf ')\n' >> $@
	@printf '\n' >> $@
	@printf 'install(TARGETS ttlcd DESTINATION bin)\n' >> $@

# =============================================================================
# GitHub Actions release workflow
# =============================================================================
$(WORKFLOW): | $(GH_WORKFLOW)
	@echo "Generating $@"
	@printf 'name: Release\n\n' > $@
	@printf 'on:\n  push:\n    tags:\n      - '"'"'v*'"'"'\n\n' >> $@
	@printf 'jobs:\n' >> $@
	@printf '  build-deb:\n' >> $@
	@printf '    runs-on: ubuntu-latest\n' >> $@
	@printf '    steps:\n' >> $@
	@printf '      - uses: actions/checkout@v4\n' >> $@
	@printf '      - name: Install dependencies\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          sudo apt-get update\n' >> $@
	@printf '          sudo apt-get install -y build-essential pkg-config dpkg-dev \\\n' >> $@
	@printf '            libusb-1.0-0-dev libopencv-dev libfreetype6-dev\n' >> $@
	@printf '      - name: Build\n' >> $@
	@printf '        run: make -j$$(nproc)\n' >> $@
	@printf '      - name: Package .deb\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          mkdir -p pkg/DEBIAN pkg/usr/bin\n' >> $@
	@printf '          cp packaging/debian/control pkg/DEBIAN/control\n' >> $@
	@printf '          sed -i "s/Version: .*/Version: $${GITHUB_REF_NAME#v}/" pkg/DEBIAN/control\n' >> $@
	@printf '          cp ttlcd pkg/usr/bin/ttlcd\n' >> $@
	@printf '          dpkg-deb --build pkg ttlcd-$${{ github.ref_name }}-amd64.deb\n' >> $@
	@printf '      - uses: actions/upload-artifact@v4\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          name: deb-package\n' >> $@
	@printf '          path: "*.deb"\n\n' >> $@
	@printf '  build-rpm:\n' >> $@
	@printf '    runs-on: ubuntu-latest\n' >> $@
	@printf '    container: fedora:latest\n' >> $@
	@printf '    steps:\n' >> $@
	@printf '      - uses: actions/checkout@v4\n' >> $@
	@printf '      - name: Install dependencies\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          dnf install -y gcc-c++ make pkg-config rpm-build \\\n' >> $@
	@printf '            libusb1-devel opencv-devel freetype-devel\n' >> $@
	@printf '      - name: Build RPM\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          mkdir -p ~/rpmbuild/{SOURCES,SPECS}\n' >> $@
	@printf '          tar czf ~/rpmbuild/SOURCES/ttlcd-$${GITHUB_REF_NAME#v}.tar.gz \\\n' >> $@
	@printf '            --transform "s,^,ttlcd-$${GITHUB_REF_NAME#v}/," .\n' >> $@
	@printf '          cp packaging/rpm/ttlcd.spec ~/rpmbuild/SPECS/\n' >> $@
	@printf '          sed -i "s/^Version:.*/Version: $${GITHUB_REF_NAME#v}/" ~/rpmbuild/SPECS/ttlcd.spec\n' >> $@
	@printf '          rpmbuild -ba ~/rpmbuild/SPECS/ttlcd.spec\n' >> $@
	@printf '          find ~/rpmbuild/RPMS -name "*.rpm" -exec cp {} . \;\n' >> $@
	@printf '      - uses: actions/upload-artifact@v4\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          name: rpm-package\n' >> $@
	@printf '          path: "*.rpm"\n\n' >> $@
	@printf '  build-arch:\n' >> $@
	@printf '    runs-on: ubuntu-latest\n' >> $@
	@printf '    container: archlinux:latest\n' >> $@
	@printf '    steps:\n' >> $@
	@printf '      - uses: actions/checkout@v4\n' >> $@
	@printf '      - name: Install dependencies\n' >> $@
	@printf '        run: pacman -Sy --noconfirm base-devel pkg-config libusb opencv freetype2\n' >> $@
	@printf '      - name: Build & package\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          cp packaging/arch/PKGBUILD .\n' >> $@
	@printf '          sed -i "s/pkgver=.*/pkgver=$${GITHUB_REF_NAME#v}/" PKGBUILD\n' >> $@
	@printf '          useradd -m builder\n' >> $@
	@printf '          chown -R builder:builder .\n' >> $@
	@printf '          su builder -c "makepkg -s --noconfirm --skipinteg"\n' >> $@
	@printf '          tar czf ttlcd-$${{ github.ref_name }}-PKGBUILD.tar.gz PKGBUILD\n' >> $@
	@printf '      - uses: actions/upload-artifact@v4\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          name: arch-package\n' >> $@
	@printf '          path: |\n' >> $@
	@printf '            *.pkg.tar.zst\n' >> $@
	@printf '            *.tar.gz\n\n' >> $@
	@printf '  release:\n' >> $@
	@printf '    needs: [build-deb, build-rpm, build-arch]\n' >> $@
	@printf '    runs-on: ubuntu-latest\n' >> $@
	@printf '    permissions:\n' >> $@
	@printf '      contents: write\n' >> $@
	@printf '    steps:\n' >> $@
	@printf '      - uses: actions/download-artifact@v4\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          merge-multiple: true\n' >> $@
	@printf '      - name: Create GitHub Release\n' >> $@
	@printf '        uses: softprops/action-gh-release@v2\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          files: |\n' >> $@
	@printf '            *.deb\n' >> $@
	@printf '            *.rpm\n' >> $@
	@printf '            *.pkg.tar.zst\n' >> $@
	@printf '            *.tar.gz\n' >> $@
	@printf '          generate_release_notes: true\n' >> $@

# =============================================================================
# Directory creation
# =============================================================================
$(PKG_DEB_DIR) $(PKG_RPM_DIR) $(PKG_ARCH_DIR) $(GH_WORKFLOW):
	mkdir -p $@

# =============================================================================
# Clean
# =============================================================================
clean:
	$(RM) $(OBJECTS) $(SOURCES:.cpp=.d) $(TARGET) $(VERSION_FILE) $(VERSION_PATCH_FILE)

clean-packaging:
	$(RM) $(CONTROL) $(SPEC) $(PKGBUILD) $(CMAKE) $(WORKFLOW)

clean-all: clean clean-packaging

.PHONY: all clean clean-packaging clean-all version packaging
