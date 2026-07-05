# 簡易 GStreamer RTSP 伺服器

## 專案簡介

本專案使用 GStreamer 和 C++ 實作一個跨平台的 RTSP (Real-Time Streaming Protocol) 伺服器與客戶端。

- **伺服器 (`rtsp_server`)**：擷取攝影機畫面，以 H.264 編碼後透過 RTSP 對外串流。
  - Windows：使用 `mfvideosrc`（Media Foundation）
  - macOS：使用 `avfvideosrc`（AVFoundation）
  - Linux：使用 `v4l2src`（V4L2），攝影機需輸出 MJPG 格式
- **客戶端 (`rtsp_client`)**：連接 RTSP 串流並顯示影像。

所有連線的客戶端共用同一路串流，不會重複開啟攝影機。

---

## 專案結構

```
simple_gst_rtsp_proj/
├── rtsp_config.h      # 共用設定（主機、埠號、路徑、影像參數）
├── rtsp_server.cpp    # RTSP 伺服器主程式
├── rtsp_client.cpp    # RTSP 客戶端主程式
├── CMakeLists.txt     # 跨平台 CMake 建置腳本
└── build/             # 編譯輸出目錄
```

## 設定

所有串流參數統一在 `rtsp_config.h` 設定：

| 參數 | 預設值 | 說明 |
|---|---|---|
| `RTSP_HOST` | `0.0.0.0` | 伺服器監聽位址 |
| `RTSP_PORT` | `8554` | 伺服器監聽埠號 |
| `RTSP_PATH` | `/test` | 串流路徑 |
| `VIDEO_DEVICE_INDEX` | `0` | 攝影機索引（Windows） |
| `VIDEO_DEVICE` | `/dev/video0` | 攝影機裝置（Linux） |
| `CAM_WIDTH` / `CAM_HEIGHT` | `1280 x 720` | 擷取解析度 |
| `VIDEO_FPS` | `30` | 擷取幀率 |
| `H264_BITRATE` | `2000` kbps | H.264 編碼位元率 |
| `H264_KEY_INT` | `10` frames | IDR 關鍵幀間隔 |

---

## 環境需求與安裝

### Windows（MinGW）

1. 安裝 [GStreamer MinGW 版本](https://gstreamer.freedesktop.org/download/)（選擇 `MinGW 64-bit` 的 runtime 與 development installer）。
2. 安裝 [CMake](https://cmake.org/download/)。
3. 確認 MinGW 的 `bin` 目錄（例如 `C:\mingw64\bin`）已加入 `PATH`。
4. **重要**：GStreamer 的 `bin` 目錄必須**置於** `PATH` 的最前面，否則可能因 DLL 版本衝突導致 plugin 載入失敗：

   ```powershell
   # 臨時設定（PowerShell）
   $env:PATH = "C:\gstreamer\1.0\mingw_x86_64\bin;" + $env:PATH
   ```

   或設定系統環境變數 `GSTREAMER_1_0_ROOT_MINGW_X86_64` 指向 GStreamer 安裝根目錄，CMakeLists.txt 會自動讀取。

### macOS（Homebrew）

透過 [Homebrew](https://brew.sh) 安裝 GStreamer 及其外掛程式與建置工具。攝影機來源使用 `avfvideosrc`（AVFoundation）。

```bash
# 安裝 GStreamer 全套件（含 rtsp-server、avfvideosrc、x264enc 等）與建置工具
brew install gstreamer cmake pkg-config
```

> **攝影機權限**：首次執行伺服器時，macOS 會要求授權終端機（Terminal / iTerm）存取攝影機。
> 若未跳出提示或被拒絕，請至「系統設定 → 隱私權與安全性 → 相機」手動勾選你的終端機程式。

確認 macOS 攝影機外掛可用：

```bash
gst-inspect-1.0 avfvideosrc          # 攝影機來源
gst-inspect-1.0 x264enc              # H.264 編碼器
gst-device-monitor-1.0 Video/Source  # 列出可用攝影機與支援解析度
```

### Linux（Debian / Ubuntu）

```bash
sudo apt-get update

# GStreamer 核心與 RTSP Server 開發函式庫
sudo apt-get install -y \
    libgstreamer1.0-dev \
    libgstrtspserver-1.0-dev

# 必要 plugin（影像來源、編解碼器）
sudo apt-get install -y \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav

# 除錯工具（建議安裝）
sudo apt-get install -y gstreamer1.0-tools
```

安裝後可驗證關鍵元件：

```bash
gst-inspect-1.0 x264enc
gst-inspect-1.0 v4l2src
gst-inspect-1.0 videotestsrc
```

---

## 編譯

在專案根目錄執行（Windows 與 Linux 通用）：

```bash
cd build
cmake ..
```

**Windows（PowerShell）：**

```powershell
cmake --build .
# 或直接使用 mingw32-make
mingw32-make
```

**Linux：**

```bash
make
```

編譯成功後，`build/` 目錄下會產生 `rtsp_server.exe` / `rtsp_server` 和 `rtsp_client.exe` / `rtsp_client`。

---

## 執行

### 啟動伺服器

```bash
# Linux
./rtsp_server

# Windows（PowerShell）
.\rtsp_server.exe
```

伺服器啟動後會印出 pipeline 內容與串流 URL：

```
[Pipeline] ( mfvideosrc device-index=0 ! ... )
RTSP Server running at rtsp://0.0.0.0:8554/test
```

### 啟動客戶端

```bash
# Linux
./rtsp_client

# Windows（PowerShell）
.\rtsp_client.exe
```

---

## 連接其他客戶端

> **區域網路連線**：伺服器綁定 `0.0.0.0:8554`，同網段的其他裝置可用本機的 LAN IP 連接，
> 例如 `rtsp://<本機IP>:8554/test`。在 macOS 上可用 `ipconfig getifaddr en0` 查詢本機 IP。

**VLC 播放器**：開啟「網路串流」，輸入：
```
rtsp://127.0.0.1:8554/test
```

**gst-launch-1.0：**
```bash
gst-launch-1.0 rtspsrc location=rtsp://127.0.0.1:8554/test ! decodebin ! autovideosink
```

**Python OpenCV：**
```python
import cv2

cap = cv2.VideoCapture("rtsp://127.0.0.1:8554/test")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break
    cv2.imshow("RTSP Stream", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
```

> OpenCV 需在啟用 GStreamer 支援的情況下編譯，才能正確解碼 RTSP 串流。

---

## 疑難排解

### Windows：`make` 指令找不到

PowerShell 沒有內建 `make`，請改用：

```powershell
cmake --build .   # 建議方式
# 或
mingw32-make
```

### Windows：DLL 版本衝突 / plugin 無法載入

**原因**：`PATH` 中存在多個 GStreamer 安裝（如 Qt 附帶的版本），系統載入了錯誤版本的 DLL。

**解決**：確保目標 GStreamer 的 `bin` 路徑置於 `PATH` **最前面**：

```powershell
$env:PATH = "C:\gstreamer\1.0\mingw_x86_64\bin;" + $env:PATH
.\rtsp_server.exe
```

### Linux：`symbol lookup error`

**症狀：**
```
symbol lookup error: /snap/core20/current/lib/x86_64-linux-gnu/libpthread.so.0: undefined symbol: __libc_pthread_init, version GLIBC_PRIVATE
```

**原因**：`LD_LIBRARY_PATH` 被 ROS 的設定腳本修改，導致載入了 Snap 套件中不相容的函式庫版本。

**解決方案：**

- 臨時方案：執行前清除 `LD_LIBRARY_PATH`：
  ```bash
  LD_LIBRARY_PATH="" ./rtsp_server
  ```

- 永久方案：在 `~/.bashrc` 中將 `source /opt/ros/<distro>/setup.bash` 那行註解掉，僅在需要使用 ROS 時手動 source。
