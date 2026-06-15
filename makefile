# =============================================================================
# TTLCD-CPP Makefile
# Target: Linux only, built with GCC
# =============================================================================

# Compiler and flags
CXX      := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic
CXXFLAGS += -Wno-deprecated-enum-enum-conversion
CPPFLAGS := -Iinclude $(shell pkg-config --cflags libusb-1.0 opencv4)
# Link only the OpenCV modules we use (avoids pulling GDAL/GDCM/etc. at runtime).
LDFLAGS  := $(shell pkg-config --libs libusb-1.0) -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_freetype
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
# Embed font binary into C++ header
# =============================================================================
EMBED_FONT_SRC := fonts/times-new-roman.ttf
EMBED_FONT_HDR := src/EmbeddedFont.hpp

embed-font: $(EMBED_FONT_HDR)

$(EMBED_FONT_HDR): $(EMBED_FONT_SRC)
	@echo "Generating $@ from $<"
	@python3 scripts/embed_font.py $(EMBED_FONT_SRC) $(EMBED_FONT_HDR)

# =============================================================================
# Default target: build everything
# =============================================================================
all: $(TARGET) packaging

packaging: $(CONTROL) $(SPEC) $(PKGBUILD) $(CMAKE) $(WORKFLOW)
	@echo "All packaging files generated."

OTHER_OBJECTS := $(filter-out main.o,$(OBJECTS))

# =============================================================================
# Version header (auto-increments patch on each successful link)
# =============================================================================
increment-version:
	@PATCH=$$(($$(cat $(VERSION_PATCH_FILE) 2>/dev/null || echo 0) + 1)); \
	echo "Generating $(VERSION_FILE) (v$(VERSION_MAJOR).$(VERSION_MINOR).$$PATCH)"; \
	echo $$PATCH > $(VERSION_PATCH_FILE); \
	echo '#pragma once' > $(VERSION_FILE); \
	echo '' >> $(VERSION_FILE); \
	echo '#include <string>' >> $(VERSION_FILE); \
	echo '' >> $(VERSION_FILE); \
	echo 'namespace Version {' >> $(VERSION_FILE); \
	echo '    constexpr const char* BUILD_DATE = "$(shell date '+%B %d, %Y')";' >> $(VERSION_FILE); \
	echo '    constexpr int MAJOR = $(VERSION_MAJOR);' >> $(VERSION_FILE); \
	echo '    constexpr int MINOR = $(VERSION_MINOR);' >> $(VERSION_FILE); \
	echo "    constexpr int PATCH = $$PATCH;" >> $(VERSION_FILE); \
	echo '' >> $(VERSION_FILE); \
	echo '    inline std::string get_version() {' >> $(VERSION_FILE); \
	echo '        return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(PATCH);' >> $(VERSION_FILE); \
	echo '    }' >> $(VERSION_FILE); \
	echo '' >> $(VERSION_FILE); \
	echo '    inline std::string get_full_info() {' >> $(VERSION_FILE); \
	echo '        return "ttlcd-cpp v" + get_version() + " (built " + BUILD_DATE + ")";' >> $(VERSION_FILE); \
	echo '    }' >> $(VERSION_FILE); \
	echo '}' >> $(VERSION_FILE)

version: increment-version

# =============================================================================
# Build
# =============================================================================
$(TARGET): $(OTHER_OBJECTS)
	@$(MAKE) --no-print-directory increment-version
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c main.cpp -o main.o
	$(CXX) $(CXXFLAGS) main.o $(OTHER_OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
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
	@printf 'Depends: libusb-1.0-0, libstdc++6, libgcc-s1 | libgcc1, libfreetype6, fontconfig, libharfbuzz0b | libharfbuzz0, libjpeg62-turbo | libjpeg-turbo8 | libjpeg8, libpng16-16t64 | libpng16-16, zlib1g, libopencv-core4.5d | libopencv-core406 | libopencv-core410, libopencv-imgproc4.5d | libopencv-imgproc406 | libopencv-imgproc410, libopencv-imgcodecs4.5d | libopencv-imgcodecs406 | libopencv-imgcodecs410, libopencv-contrib4.5d | libopencv-contrib406 | libopencv-contrib410\n' >> $@
	@printf 'Build-Depends: build-essential, g++, make, pkg-config, python3, libusb-1.0-0-dev, libopencv-dev, libopencv-contrib-dev, libfreetype6-dev, libfontconfig-dev, libharfbuzz-dev, libjpeg-dev, libpng-dev, zlib1g-dev\n' >> $@
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
	@printf '%%global debug_package %%{nil}\n' >> $@
	@printf 'Summary:        TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels\n' >> $@
	@printf 'License:        MIT\n' >> $@
	@printf 'URL:            https://github.com/AaronC-585/ttlcd-cpp\n' >> $@
	@printf 'Source0:        %%{name}-%%{version}.tar.gz\n' >> $@
	@printf '\n' >> $@
	@printf 'BuildRequires:  gcc-c++ make pkg-config python3 libusb1-devel opencv-devel freetype-devel libpng-devel libjpeg-turbo-devel zlib-devel fontconfig-devel harfbuzz-devel\n' >> $@
	@printf 'Requires:       libusb1 libstdc++ libgcc libfreetype fontconfig harfbuzz libpng libjpeg-turbo zlib opencv-core opencv-imgproc opencv-imgcodecs opencv-freetype\n' >> $@
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
	@echo '* '$$(LC_ALL=C date -u '+%a %b %d %Y')' Aaron C <176900889+AaronC-585@users.noreply.github.com> - $(VERSION)-1' >> $@
	@echo '- Build $(VERSION)' >> $@

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
	@printf 'url="https://github.com/AaronC-585/ttlcd-cpp"\n' >> $@
	@printf "license=('MIT')\n" >> $@
	@printf "depends=('libusb' 'opencv' 'freetype2' 'fontconfig' 'harfbuzz' 'libjpeg-turbo' 'libpng' 'zlib' 'gcc-libs')\n" >> $@
	@printf "makedepends=('base-devel' 'pkg-config' 'python' 'libusb' 'opencv' 'freetype2' 'fontconfig' 'harfbuzz' 'libjpeg-turbo' 'libpng' 'zlib')\n" >> $@
	@printf 'source=("$$pkgname-$$pkgver.tar.gz")\n' >> $@
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
	@printf '    opencv_core opencv_imgproc opencv_imgcodecs opencv_freetype\n' >> $@
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
	@printf 'on:\n  push:\n    tags:\n      - '"'"'v*'"'"'\n  workflow_dispatch:\n    inputs:\n      version:\n        description: '"'"'Release version tag (e.g. v1.0.18)'"'"'\n        required: true\n        type: string\n\n' >> $@
	@printf 'jobs:\n' >> $@
	@printf '  build-deb:\n' >> $@
	@printf '    runs-on: ubuntu-latest\n' >> $@
	@printf '    steps:\n' >> $@
	@printf '      - uses: actions/checkout@v4\n' >> $@
	@printf '      - name: Install dependencies\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          sudo apt-get update\n' >> $@
	@printf '          sudo apt-get install -y build-essential g++ make pkg-config dpkg-dev python3 \\\n' >> $@
	@printf '            libusb-1.0-0-dev libopencv-dev libopencv-contrib-dev libfreetype6-dev \\\n' >> $@
	@printf '            libfontconfig-dev libharfbuzz-dev libjpeg-dev libpng-dev zlib1g-dev\n' >> $@
	@printf '      - name: Build\n' >> $@
	@printf '        run: make -j$$(nproc)\n' >> $@
	@printf '      - name: Package .deb\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          VER="$${GITHUB_REF_NAME#v}"\n' >> $@
	@printf '          echo "$${VER##*.}" > .build_patch\n' >> $@
	@printf '          make deb-package DEB_FILE=ttlcd-$${{ github.ref_name }}-amd64.deb\n' >> $@
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
	@printf '          dnf install -y gcc-c++ make pkg-config rpm-build python3 \\\n' >> $@
	@printf '            libusb1-devel opencv-devel freetype-devel libpng-devel \\\n' >> $@
	@printf '            libjpeg-turbo-devel zlib-devel fontconfig-devel harfbuzz-devel\n' >> $@
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
	@printf '        run: pacman -Sy --noconfirm base-devel pkg-config python libusb opencv freetype2 fontconfig harfbuzz libjpeg-turbo libpng zlib\n' >> $@
	@printf '      - name: Build & package\n' >> $@
	@printf '        run: |\n' >> $@
	@printf '          VER="$${GITHUB_REF_NAME#v}"\n' >> $@
	@printf '          cp packaging/arch/PKGBUILD .\n' >> $@
	@printf '          sed -i "s/pkgver=.*/pkgver=$$VER/" PKGBUILD\n' >> $@
	@printf '          tar czf ttlcd-$$VER.tar.gz --transform "s,^,ttlcd-$$VER/," --exclude="ttlcd-*.tar.gz" --exclude=".git" .\n' >> $@
	@printf '          useradd -m builder\n' >> $@
	@printf '          chown -R builder:builder .\n' >> $@
	@printf '          su builder -c "makepkg -s --noconfirm --skipinteg --noprogressbar"\n' >> $@
	@printf '          tar czf ttlcd-$${{ github.ref_name }}-PKGBUILD.tar.gz PKGBUILD\n' >> $@
	@printf '      - uses: actions/upload-artifact@v4\n' >> $@
	@printf '        with:\n' >> $@
	@printf '          name: arch-package\n' >> $@
	@printf '          path: |\n' >> $@
	@printf '            *.pkg.tar.zst\n' >> $@
	@printf '            *.tar.gz\n\n' >> $@
	@printf '  release:\n' >> $@
	@printf '    needs: [build-deb, build-rpm, build-arch]\n' >> $@
	@printf '    if: always() && !cancelled() && needs.build-deb.result == '"'"'success'"'"' && needs.build-rpm.result == '"'"'success'"'"'\n' >> $@
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
# Debian package (binary + systemd service + udev rule + default config)
# =============================================================================
DEB_STAGING := pkg
DEB_VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(shell cat $(VERSION_PATCH_FILE) 2>/dev/null || echo 0)
DEB_FILE    ?= ttlcd-v$(DEB_VERSION)-amd64.deb

deb: $(TARGET) $(CONTROL)
	@$(MAKE) --no-print-directory deb-package

deb-package: $(CONTROL)
	@test -f $(TARGET) || (echo "Missing $(TARGET); run make first." && exit 1)
	@echo "Building $(DEB_FILE)"
	@rm -rf $(DEB_STAGING)/usr $(DEB_STAGING)/etc $(DEB_STAGING)/lib
	@mkdir -p $(DEB_STAGING)/DEBIAN \
		$(DEB_STAGING)/usr/bin \
		$(DEB_STAGING)/usr/lib/systemd/system \
		$(DEB_STAGING)/usr/share/ttlcd/backgrounds \
		$(DEB_STAGING)/etc/ttlcd \
		$(DEB_STAGING)/lib/udev/rules.d
	@cp $(CONTROL) $(DEB_STAGING)/DEBIAN/control
	@sed -i 's/^Version:.*/Version: $(DEB_VERSION)/' $(DEB_STAGING)/DEBIAN/control
	@install -Dm755 $(TARGET) $(DEB_STAGING)/usr/bin/ttlcd
	@install -Dm644 packaging/debian/ttlcd.service $(DEB_STAGING)/usr/lib/systemd/system/ttlcd.service
	@install -Dm644 packaging/debian/ttlcd.sysusers $(DEB_STAGING)/usr/lib/sysusers.d/ttlcd.conf
	@install -Dm644 packaging/debian/99-ttlcd.rules $(DEB_STAGING)/lib/udev/rules.d/99-ttlcd.rules
	@install -Dm644 packaging/debian/ttlcd.default.json $(DEB_STAGING)/etc/ttlcd/config.json
	@install -Dm644 backgrounds/permanent-headers.jpg $(DEB_STAGING)/usr/share/ttlcd/backgrounds/permanent-headers.jpg
	@install -Dm755 packaging/debian/postinst $(DEB_STAGING)/DEBIAN/postinst
	@install -Dm755 packaging/debian/prerm $(DEB_STAGING)/DEBIAN/prerm
	@install -Dm755 packaging/debian/postrm $(DEB_STAGING)/DEBIAN/postrm
	@printf '/etc/ttlcd/config.json\n' > $(DEB_STAGING)/DEBIAN/conffiles
	@dpkg-deb --root-owner-group --build $(DEB_STAGING) $(DEB_FILE)
	@echo "Created $(DEB_FILE)"

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

.PHONY: all clean clean-packaging clean-all version increment-version packaging embed-font deb deb-package
