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

## D80N 實機串流規格

本專案用來在任何有 webcam 的電腦上模擬 D80N RTSP 攝影機，預設參數皆對齊下表。
以下為 2026-10-27 對實機 `rtsp://10.223.1.115` 實測結果（`DESCRIBE` / `gst-discoverer-1.0` / 擷取 14 秒串流統計）：

| 項目 | 實測值 |
|---|---|
| RTSP 伺服器 | `lal 0.37.4`，port 554，根路徑（URL 只需 `rtsp://<IP>`），無認證 |
| 支援方法 | DESCRIBE, ANNOUNCE, SETUP, PLAY, PAUSE, RECORD, TEARDOWN |
| 傳輸 | RTP over UDP 與 TCP (interleaved) 皆可 |
| Codec | H.264 High Profile, Level 5.2 |
| RTP | payload type 96, `packetization-mode=1` |
| SDP | 含 `sprop-parameter-sets`，track control 為 `streamid=0` |
| 解析度 | 1920x1080，4:2:0，8-bit，progressive |
| 幀率 | 30 fps |
| GOP | 固定 60 frames（每 2 秒一個 IDR），IDR 前附 SPS/PPS |
| B-frame | 無（PTS 單調遞增、無 DTS） |
| Bitrate | 平均約 2150 kbps；I-frame 約 190 KB、P-frame 約 5 KB |

SDP 原文：

```
v=0
o=- 0 0 IN IP4 127.0.0.1
s=No Name
c=IN IP4 127.0.0.1
t=0 0
a=tool:lal 0.37.4
m=video 0 RTP/AVP 96
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=J2QANKwbGqB4AiflmyAAAAMAIAAAB4HixNQ=,KO4C3LA=; profile-level-id=640016
a=control:streamid=0
```

### 改成 H.265 720p30 512 kbps 後的實測

使用者在實機設定改成 `H.265 / 720p / 30fps / 512 kbps` 後重新測量（2026-10-27）：

| 項目 | 實測值 |
|---|---|
| Codec | H.265 Main Profile, Main tier, Level 6（SPS 宣告值） |
| RTP | **payload type 98**（H.264 時為 96） |
| SDP | `fmtp` 帶 `profile-id=1` 與 `sprop-vps` / `sprop-sps` / `sprop-pps` |
| 解析度 / 幀率 | 1280x720，30 fps |
| GOP | 仍為固定 60 frames（2 秒） |
| B-frame | 無（PTS 單調遞增、無 DTS） |
| Bitrate | 平均約 524 kbps；I-frame 約 51 KB、P-frame 約 1.3 KB |
| 傳輸 | UDP 與 TCP 皆可 |

```
m=video 0 RTP/AVP 98
a=rtpmap:98 H265/90000
a=fmtp:98 profile-id=1;sprop-sps=QgEBAUAAAAMAgAAAAwAAAwC0oAKAgC0WWuSytmuXE2BAAAADAEAAAAeP4sSg;sprop-pps=RAHA4IDwMyQ=;sprop-vps=QAEMAf//AUAAAAMAgAAAAwAAAwC0rAk=
a=control:streamid=0
```

## 客戶端使用方式

模擬器要讓以下兩個客戶端「只換 IP 就能用」。

### skai-edge（ROS 2 / Python，`~/poyi/skai-edge`）

`video_source_node` → `utils/video_grabber.py`：

- 先自己送一個**無認證**的 `DESCRIBE`（未指定 port 時用 554），SDP 的 `a=rtpmap` 為 `H265/` 時走 H.265，其他一律 H.264
- 接收 pipeline：
  `rtspsrc protocols=udp+tcp latency=500 timeout=5000000 ! rtp{h264,h265}depay wait-for-keyframe=true request-keyframe=true ! {h264,h265}parse ! nvv4l2decoder ! nvvidconv ! BGRx ! videoconvert ! BGR ! appsink drop=true`
- 解碼後統一縮放至設定解析度（`camera_config.yaml` 預設 1920x1080、30 fps）
- 只用解碼後的畫面做推論，不轉送原始 bitstream
- 另有 D80N 雲台控制（`utils/d80n_controller.py`，私有協定，UDP 2337/2338 或 TCP 2332），預設 `gimbal_enable: false`，**本模擬器未實作**

### skai-edge2（C++17 單一執行檔，`~/poyi/skai-edge2`）

`src/video/rtsp_source.cpp`，設定在 `config.yaml` 的 `video:`：

- `rtspsrc` 參數：`transport` 目前預設 **`tcp`**（只用一種）、`latency_ms` 100，可設 `username` / `password`；
  **之後會改成跟 skai-edge 一樣先試 UDP、失敗再退回 TCP**，所以模擬器兩種傳輸都要支援
- codec 由 `rtspsrc` pad 的 RTP caps `encoding-name` 判斷，只接受 `H264` / `H265`
- pipeline：`rtp{h264,h265}depay ! {h264,h265}parse config-interval=-1 ! byte-stream,au ! tee`
  - 解碼支線：`nvv4l2decoder`（沒有時用 `avdec_h264/h265`）→ BGR → 推論；H.264 會設 `disable-dpb=true`，
    **有 B-frame 時畫面順序會錯**（不會報錯），所以來源不能有 B-frame
  - 原始 bitstream 支線（**不重新編碼**，直接轉送）：
    - **Agora**：只送 H.264，**H.265 會整個略過**（Skai 網頁的 Agora Web SDK 播不了 H.265）
    - **WebRTC (WHEP)**：照來源 codec 協商，H.265 要觀看端瀏覽器支援；從下一張 IDR 開始送，丟包也等下一張 IDR
    - **MP4 分段錄影**：分段只能在 IDR 切，所以 GOP 決定最短分段長度
- 斷線偵測：`stall_timeout_ms` 3 秒沒畫面就重連；`first_frame_timeout_ms` 預設 35 秒（為了容忍 30 秒 GOP 的 camera）

skai-edge2 把 `10.223.1.115` 這台稱為 **D80**，同一份紀錄（`TODO.md`）另有 **D80 Pro**（`10.223.1.111`，大華方案）可對照：

| | D80（本模擬器目標） | D80 Pro |
|---|---|---|
| H.264 | High L5.2，1080p30，1 個參考幀 | High L5.0，1080p24，2 個參考幀 |
| 位元率 | 約 2.1 Mbps | VBR 0.06–1.4 Mbps |
| GOP | 2 秒 | 預設約 30 秒（`SmartEncode`），已關閉改回 2 秒 |
| SPS `bitstream_restriction` | 有，`max_dec_frame_buffering=1` | 沒有 → iPhone WebRTC 黑畫面 |

### 對模擬器的要求

- port 554、根路徑、不需認證，UDP 與 TCP 都要能拉
- 不能有 B-frame（`tune=zerolatency`）；IDR 前要帶 SPS/PPS（`config-interval=-1`）
- GOP 維持 2 秒左右，否則首畫面、錄影分段、WebRTC 恢復都會變慢
- SPS 需帶 `bitstream_restriction`（x264 預設會寫），否則 iPhone 播不出來
- `codec: h265` 只適合測 H.265 路徑：skai-edge2 的 Agora 會停止送流

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
   # 臨時設定（PowerShell）— 使用安裝時已設好的環境變數，不必寫死路徑
   $env:PATH = "$env:GSTREAMER_1_0_ROOT_MINGW_X86_64\bin;" + $env:PATH
   ```

   > GStreamer MinGW 安裝程式會自動設定 `GSTREAMER_1_0_ROOT_MINGW_X86_64` 指向安裝根目錄
   > （預設 `C:\Program Files\gstreamer\1.0\mingw_x86_64`）。CMakeLists.txt 也會讀取此變數。
   > 若要寫死路徑，請改成你實際的安裝位置，別直接抄範例路徑。

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

**症狀**：執行時出現
`Failed to load plugin '...libgstmediafoundation.dll': The specified procedure could not be found.`
接著一連串 `GStreamer-CRITICAL` 錯誤，`mfvideosrc` 無法建立。

**原因**：`PATH` 上排在 GStreamer 前面的其他工具鏈（最常見是 **Qt 內附的 MinGW**，如 `C:\Qt\...\mingw73_64\bin`）提供了舊版的 `libstdc++-6.dll` / `libwinpthread-1.dll` / `libgcc_s_seh-1.dll`，被搶先載入，導致 GStreamer 新版 plugin 找不到需要的符號。
（註：`gst-inspect-1.0` 檢查同一個 plugin 可能正常，因為它就在 `gstreamer\bin` 內，會優先載入正確的 DLL；但 `rtsp_server.exe` 在 `build\`，只能沿 PATH 找，才會撞到舊版。）

**解決**：把 GStreamer 的 `bin` 置於 `PATH` **最前面**，讓它的 runtime DLL 勝出：

```powershell
$env:PATH = "$env:GSTREAMER_1_0_ROOT_MINGW_X86_64\bin;" + $env:PATH
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
