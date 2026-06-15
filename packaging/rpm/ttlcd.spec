Name:           ttlcd
Version:        1.0.2
Release:        1%{?dist}
%global debug_package %{nil}
Summary:        TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels
License:        MIT
URL:            https://github.com/AaronC-585/ttlcd-cpp
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ make pkg-config python3 libusb1-devel opencv-devel freetype-devel libpng-devel libjpeg-turbo-devel zlib-devel fontconfig-devel harfbuzz-devel
Requires:       libusb1 libstdc++ libgcc libfreetype fontconfig harfbuzz libpng libjpeg-turbo zlib opencv-core opencv-imgproc opencv-imgcodecs opencv-freetype

%description
A modern C++ utility for driving 3.9-inch USB LCD panels (480x128) with
live system monitoring, custom TTF fonts via OpenCV FreeType, bar graphs,
network speed, and full JSON configuration.

%prep
%autosetup

%build
make -j$(nproc)

%install
install -Dm755 ttlcd %{buildroot}%{_bindir}/ttlcd

%files
%{_bindir}/ttlcd

%changelog
* Mon Jun 15 2026 Aaron C <176900889+AaronC-585@users.noreply.github.com> - 1.0.2-1
- Build 1.0.2
