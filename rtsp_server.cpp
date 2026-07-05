#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>
#include <iostream>
#include <sstream>
#include "rtsp_config.h"

// 建立與接收端相容的 pipeline 字串
// 接收端使用: rtspsrc ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvidconv
// 因此 server 需要輸出標準 H.264 RTP 串流
static std::string build_pipeline() {
    std::ostringstream oss;

#ifdef _WIN32
    // Windows: 使用 mfvideosrc (Media Foundation)
    // 輸出 raw video，由 videoconvert 轉換色彩空間供 x264enc 使用
    oss << "( "
        // 使用 JPEG 輸出：1280x720 raw(YUY2) 最高只有 10fps，
        // JPEG 格式才支援 30fps，避免相機降頻導致串流 freeze
        << "mfvideosrc device-index=" << VIDEO_DEVICE_INDEX << " do-timestamp=true ! "
        << "image/jpeg,width=" << CAM_WIDTH
        << ",height=" << CAM_HEIGHT
        << ",framerate=" << VIDEO_FPS << "/1 ! "
        << "jpegdec ! "
        << "videoconvert ! ";
#else
    // Linux: 使用 v4l2src，攝影機輸出 MJPG，解碼後轉換色彩空間
    oss << "( "
        << "v4l2src device=" << VIDEO_DEVICE << " ! "
        << "image/jpeg,width=" << CAM_WIDTH
        << ",height=" << CAM_HEIGHT
        << ",framerate=" << VIDEO_FPS << "/1 ! "
        << "jpegdec ! "
        << "videoconvert ! ";
#endif

    // 共用部分：H.264 編碼 + RTP 封包
    //
    // tune 可選值（未設定則使用預設，啟用 lookahead + B-frame，畫質較佳但延遲較高）:
    //   stillimage  : 靜態圖片優化，增加壓縮延遲，不適合串流
    //   animation   : 卡通/平滑色塊動畫優化，對延遲無特別影響
    //   grain       : 保留底片顆粒感，對延遲無特別影響
    //   psnr        : 最大化 PSNR 指標（測試用），增加延遲
    //   ssim        : 最大化 SSIM 指標（測試用），增加延遲
    //   fastdecode  : 降低接收端解碼 CPU 用量，對編碼延遲無影響
    //   zerolatency : 關閉 lookahead 與 B-frame，每幀立即輸出，延遲最低
    //
    // speed-preset 可選值（相對速度以 medium=1x 為基準；壓縮效率為同畫質下相對 veryslow 的額外 bitrate 開銷）:
    //   ultrafast : ~16x，壓縮效率 -50%
    //   superfast : ~10x，壓縮效率 -35%
    //   veryfast  :  ~6x，壓縮效率 -22%
    //   faster    : ~2.5x，壓縮效率 -18%（目前使用）
    //   fast      : ~1.7x，壓縮效率 -14%
    //   medium    :    1x，壓縮效率 -10%（預設值）
    //   slow      : ~0.5x，壓縮效率  -5%
    //   slower    : ~0.25x，壓縮效率 -3%
    //   veryslow  : ~0.13x，壓縮效率  0%（最佳基準）
    // key-int-max : 控制 IDR 關鍵幀間隔
    oss << "x264enc"
        << " tune=zerolatency"
        << " speed-preset=faster"
        << " bitrate=" << H264_BITRATE
        << " key-int-max=" << H264_KEY_INT << " ! "

        // h264parse : 正規化 H.264 bitstream，確保 NAL 對齊與 timestamp 正確，
        //             對 nvv4l2decoder 等硬體解碼器的相容性較好
        << "h264parse ! "

        // RTP 封包
        // config-interval=-1 : 每個 IDR frame 都附帶 SPS/PPS，
        //                       讓接收端隨時加入都能正確解碼
        // pt=96              : 動態 payload type，標準 H.264 慣例
        << "rtph264pay name=pay0 pt=96 config-interval=-1"
        << " )";

    return oss.str();
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // 建立 RTSP Server
    GstRTSPServer *server = gst_rtsp_server_new();
    gst_rtsp_server_set_address(server, RTSP_HOST);
    gst_rtsp_server_set_service(server, RTSP_PORT);

    // 建立 Media Factory
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    std::string pipeline = build_pipeline();
    std::cout << "[Pipeline] " << pipeline << std::endl;

    gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());

    // shared=TRUE：所有連線的 client 共用同一路串流，不重複開啟攝影機
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(mounts, RTSP_PATH, factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(server, NULL);

    std::cout << "RTSP Server running at " << RTSP_URL << std::endl;

    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(server);
    return 0;
}
