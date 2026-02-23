Name:           ttlcd
Version:        1.0.1
Release:        1%{?dist}
Summary:        TTL LCD system monitor for Thermaltake Tower 200 and compatible USB LCD panels
License:        MIT
URL:            https://github.com/AaronC-585/ttlcd
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ make pkg-config libusb1-devel opencv-devel freetype-devel
Requires:       libusb1 opencv freetype

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
* 0x0p+0  0 