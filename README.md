# 簡易 GStreamer RTSP 伺服器

## 專案簡介
本專案使用 GStreamer 和 C++ 實作一個基礎的 RTSP (Real-Time Streaming Protocol) 伺服器。它可以將一個影像來源（預設使用 `videotestsrc` 進行除錯，或使用 `/dev/video0` 連接真實攝影機）透過 RTSP 進行串流，讓客戶端可以連接並觀看影像。

## 安裝與環境要求

要編譯並執行此 RTSP 伺服器，您的系統需要安裝 GStreamer、其 RTSP 伺服器開發函式庫，以及相關的外掛程式。

**適用於 Debian/Ubuntu 的系統：**

請開啟您的終端機，並執行以下指令來安裝所有必要的套件：

```bash
# 1. 更新您的套件列表
sudo apt-get update

# 2. 安裝 GStreamer 核心開發函式庫
sudo apt-get install -y libgstreamer1.0-dev

# 3. 安裝 GStreamer RTSP 伺服器開發函式庫
sudo apt-get install -y libgstrtspserver-1.0-dev

# 4. 安裝必要的 GStreamer 外掛程式 (base, good, bad, ugly)
#    這些套件提供了影像處理管線所需的編解碼器、影像來源 (如 v4l2src, videotestsrc) 
#    以及編碼器 (如 x264enc)。
sudo apt-get install -y \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav # 透過 FFmpeg 提供額外的編解碼器

# 5. 安裝 GStreamer 工具 (強烈建議，用於除錯)
sudo apt-get install -y gstreamer1.0-tools
```

### 驗證外掛程式是否安裝成功

安裝完成後，您可以使用 `gst-inspect-1.0` 工具來確認特定的 GStreamer 元件（例如 `x264enc` 或 `videotestsrc`）是否可用：

```bash
# 檢查 x264enc 是否可用 (H.264 串流的核心)
gst-inspect-1.0 x264enc

# 檢查 videotestsrc 是否可用
gst-inspect-1.0 videotestsrc

# 檢查 v4l2src 是否可用 (用於攝影機輸入)
gst-inspect-1.0 v4l2src
```
如果元件已成功安裝，`gst-inspect-1.0` 將會印出該元件的詳細資訊。如果找不到，它會顯示錯誤訊息，表示您可能遺漏了某個套件。

## 編譯專案

當所有環境要求都滿足後，請進入 `build` 目錄，並使用 CMake 來設定與編譯專案：

```bash
cd build
cmake ..
make
```

## 執行伺服器

成功編譯後，您可以從 `build` 目錄執行 RTSP 伺服器：

```bash
cd build
./rtsp_server
```
伺服器啟動後會顯示 RTSP 的 URL，通常是 `rtsp://127.0.0.1:8554/test`。

## 連接客戶端

您可以使用多種 RTSP 客戶端來連接串流，例如：

*   **VLC 播放器：** 開啟「網路串流」並輸入 `rtsp://127.0.0.1:8554/test`。
*   **GStreamer 的 `gst-launch-1.0`：**
    ```bash
    gst-launch-1.0 rtspsrc location=rtsp://127.0.0.1:8554/test ! decodebin ! autovideosink
    ```
*   **您的 Python OpenCV 客戶端：**
    ```python
    import cv2

    rtsp_url = "rtsp://127.0.0.1:8554/test"
    cap = cv2.VideoCapture(rtsp_url)

    if not cap.isOpened():
        print("錯誤：無法開啟 RTSP 串流")
    else:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("抓取影像失敗")
                break
            cv2.imshow("RTSP Stream", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        cap.release()
        cv2.destroyAllWindows()
    ```
如果您在使用 Python 客戶端時遇到問題，請確保您的 OpenCV 是在與 GStreamer 連結的情況下安裝的

## 疑難排解 (Troubleshooting)

### 執行時出現 `symbol lookup error`

**症狀**

在執行 `rtsp_server`、`rtsp_client` 或其他相關程式時，您可能會遇到類似以下的錯誤訊息：
```
symbol lookup error: /snap/core20/current/lib/x86_64-linux-gnu/libpthread.so.0: undefined symbol: __libc_pthread_init, version GLIBC_PRIVATE
```

**原因**

這個錯誤通常是由於環境變數衝突所引起的，特別是當您的 shell 環境（例如 `~/.bashrc`）中設定了來自 **ROS (Robot Operating System)** 的環境。ROS 的設定腳本會修改 `LD_LIBRARY_PATH` 變數，這可能導致程式載入了一個與系統不相容的函式庫版本（例如從 Snap 套件中載入），從而產生衝突。

**解決方案**

您可以選擇以下任一種方法來解決這個問題：

1.  **臨時解決方案：**
    在執行指令前，暫時清除 `LD_LIBRARY_PATH` 變數。這種方法不會影響您目前的 shell 環境設定。
    ```bash
    LD_LIBRARY_PATH="" ./rtsp_server
    ```
    或者
    ```bash
    LD_LIBRARY_PATH="" ./rtsp_client
    ```

2.  **永久解決方案：**
    如果您不需要在所有終端機中都啟用 ROS 環境，建議修改您的 shell 設定檔。
    *   開啟您的 `~/.bashrc` 或 `~/.zshrc` 檔案。
    *   找到類似 `source /opt/ros/<distro>/setup.bash` 的那一行。
    *   將其 **註解掉** (在行首加上 `#`) 或 **刪除**。
    *   未來只在您需要使用 ROS 的特定終端機視窗中，手動執行 `source /opt/ros/<distro>/setup.bash` 指令來啟用 ROS 環境。

這樣做可以將 ROS 的環境與其他開發工作隔離開，從根本上避免函式庫衝突。

