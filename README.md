<h1>ttlcd-cpp <span class="badge">C++</span></h1>

<p><strong>Modern C++ port of the original Python driver for the Thermaltake Tower 200 (and compatible models) 3.9-inch USB LCD panel (480×128 resolution).</strong></p>

<p>This project provides a highly customizable, performant system monitoring dashboard with live updates, custom TTF fonts, bar graphs, network speed, and more — all running natively on Linux.</p>

<hr>

<h2>Features</h2>

<ul>
    <li>Live system monitoring: CPU & RAM usage bars, network upload/download speed, load average, date/time</li>
    <li>Dynamic text widget with powerful placeholders (CPU %, RAM %, network speeds, time, etc.)</li>
    <li>Custom TTF fonts via OpenCV FreeType module (e.g., Digital-7, Orbitron for retro LCD look)</li>
    <li>Graceful error handling and fallbacks (shows "ERR" on screen if data source fails)</li>
    <li>Full JSON configuration — easy layout, color, size, and widget control</li>
    <li>Complete USB protocol support including device initialization sequence</li>
    <li>Robust libusb error reporting</li>
    <li>Temporary image rendering with automatic cleanup</li>
</ul>

<h2>Screenshot</h2>

<p><em>(Example dashboard with Digital-7 font and subtle grid background)</em></p>
<!-- Replace with actual screenshot when available -->
<!-- <img src="screenshot.png" alt="ttlcd-cpp dashboard example" style="max-width:100%; border-radius:8px;"> -->

<h2>Requirements</h2>

<h3>Runtime Dependencies</h3>
<pre><code>sudo apt install libusb-1.0-0 libopencv-core4.5 libopencv-imgproc4.5 libopencv-imgcodecs4.5 libopencv-highgui4.5</code></pre>

<h3>Build Dependencies</h3>
<pre><code>sudo apt install build-essential pkg-config libusb-1.0-0-dev libopencv-dev libfreetype6-dev</code></pre>

<h2>Building</h2>

<pre><code>make -j$(nproc)          # Build the executable
sudo ./ttlcd config.json # Run (requires sudo for USB access)</code></pre>

<p>Clean rebuild:</p>
<pre><code>make clean && make -j$(nproc)</code></pre>

<h2>Configuration (config.json)</h2>

<p>Full example configuration:</p>

<pre><code>{
  "idVendor": 9802,
  "idProduct": 9021,
  "use": "NODE",
  "orientation": "TOP",
  "background": "./bg.jpg",

  // Global defaults
  "font_size": 14,
  "font_color": [255, 255, 255],

  // Global custom font (highly recommended)
  "font_file": "fonts/Digital-7.ttf",

  // Widgets
  "enable_date": true,
  "date_x": 20, "date_y": 10, "date_font_size": 20,

  "enable_time": true,
  "time_x": 20, "time_y": 40, "time_font_size": 32,

  "enable_cpu_bar": true,
  "cpu_bar_x": 20, "cpu_bar_y": 80,
  "cpu_bar_width": 440, "cpu_bar_height": 30,

  "enable_ram_bar": true,
  "ram_bar_x": 20, "ram_bar_y": 120,
  "ram_bar_width": 440, "ram_bar_height": 30,

  "enable_network": true,
  "network_x": 20, "network_y": 160,
  "network_font_size": 28,

  "enable_loadavg": true,
  "loadavg_x": 20, "loadavg_y": 200,

  "enable_dynamic_text": true,
  "dynamic_text_x": 20, "dynamic_text_y": 230,
  "dynamic_text_font_size": 24,
  "dynamic_text_font_color": [0, 255, 255],
  "format": "CPU: {cpu_percent}% | RAM: {ram_percent}% | ↑{net_up} ↓{net_down} | {time}"
}</code></pre>

<h3>Dynamic Text Placeholders</h3>
<ul>
    <li><code>{cpu_percent}</code> — CPU usage %</li>
    <li><code>{ram_percent}</code> — RAM usage %</li>
    <li><code>{net_up}</code> / <code>{net_down}</code> — Upload/download speed (KB/s)</li>
    <li><code>{loadavg}</code> — Load average (1/5/15 min)</li>
    <li><code>{time}</code> — Current time</li>
    <li><code>{date}</code> — Current date</li>
</ul>

<h3>Custom Fonts</h3>
<p>Create a <code>fonts/</code> folder and place <code>.ttf</code> files there. Recommended free fonts:</p>
<ul>
    <li><a href="https://www.dafont.com/digital-7.font">Digital-7</a> (classic 7-segment)</li>
    <li><a href="https://www.dafont.com/ds-digital.font">DS-Digital</a></li>
    <li><a href="https://fonts.google.com/specimen/Orbitron">Orbitron</a> (futuristic)</li>
</ul>

<h2>Running as Daemon (Optional)</h2>

<p>Create <code>/etc/systemd/system/ttlcd.service</code>:</p>

<pre><code>[Unit]
Description=Thermaltake LCD Dashboard
After=network.target

[Service]
Type=simple
User=root
ExecStart=/path/to/ttlcd /path/to/config.json
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target</code></pre>

<p>Enable and start:</p>
<pre><code>sudo systemctl enable ttlcd.service
sudo systemctl start ttlcd.service</code></pre>

<h2>Troubleshooting</h2>

<ul>
    <li><strong>Black screen</strong>: Check console for "Device initialization sequence completed"</li>
    <li><strong>USB timeout</strong>: Detailed libusb error messages printed</li>
    <li><strong>No custom font</strong>: Falls back to Hershey — check font path and FreeType support</li>
    <li><strong>Permission denied</strong>: Run with <code>sudo</code></li>
</ul>

<h2>Credits & License</h2>

<p>Originally inspired by the Python driver by <a href="https://github.com/bekindpleaserewind/ttlcd">bekindpleaserewind</a>.<br>
Ported and enhanced in C++ for better performance and features.</p>

<p><strong>License</strong>: MIT — feel free to modify and share!</p>

<p>Enjoy your sleek, custom LCD dashboard! 🎉</p>

<p><em>Last updated: February 22, 2026</em></p>
