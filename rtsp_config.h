#pragma once

#define RTSP_HOST    "0.0.0.0"
// 對齊 D80N 實機：預設 RTSP port 554、根路徑，客戶端只需填 rtsp://<IP>
// Linux 綁定 554 需要 root 或 CAP_NET_BIND_SERVICE
#define RTSP_PORT    "554"
#define RTSP_PATH    "/"
#define RTSP_URL     "rtsp://" RTSP_HOST ":" RTSP_PORT RTSP_PATH

// 影像來源
#if defined(_WIN32)
    // Windows: Media Foundation 攝影機裝置索引 (0 = 第一顆攝影機)
    #define VIDEO_DEVICE_INDEX 0
#elif defined(__APPLE__)
    // macOS: AVFoundation 攝影機裝置索引 (0 = 第一顆攝影機)
    #define VIDEO_DEVICE_INDEX 0
#else
    // Linux: V4L2 裝置路徑
    #define VIDEO_DEVICE "/dev/video0"
#endif

// 攝影機擷取幀率；config.yaml 的 fps 若不同，由 videorate 轉換
#define CAM_FPS      30

// 編碼參數
// codec / resolution / fps / bitrate 改由 config.yaml 設定
#define VIDEO_KEY_INT   60     // IDR 間隔 (frames)，對齊 D80N 實機 (30fps 下 2 秒一個關鍵幀)

// 未指定設定檔時，依序搜尋目前目錄與原始碼目錄下的 config.yaml
#define CONFIG_FILE_NAME "config.yaml"
