#pragma once

#define RTSP_HOST    "0.0.0.0"
#define RTSP_PORT    "8554"
#define RTSP_PATH    "/test"
#define RTSP_URL     "rtsp://" RTSP_HOST ":" RTSP_PORT RTSP_PATH

// 影像來源
#ifdef _WIN32
    // Windows: Media Foundation 攝影機裝置索引 (0 = 第一顆攝影機)
    #define VIDEO_DEVICE_INDEX 0
#else
    // Linux: V4L2 裝置路徑
    #define VIDEO_DEVICE "/dev/video0"
#endif

#define CAM_WIDTH    1280
#define CAM_HEIGHT   720
#define VIDEO_FPS    30

// 編碼參數
#define H264_BITRATE    4000   // kbps
#define H264_KEY_INT    30     // IDR 間隔 (frames)，對應 FPS 即約 1 秒一個關鍵幀
