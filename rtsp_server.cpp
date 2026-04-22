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

    oss << "( "
        // 影像來源：V4L2 攝影機輸出 MJPG，解碼後直接編碼，不在 server 端縮放
        // client 端透過 nvvidconv caps 自行縮放至所需解析度
        << "v4l2src device=" << VIDEO_DEVICE << " ! "
        << "image/jpeg,width=" << CAM_WIDTH
        << ",height=" << CAM_HEIGHT << " ! "
        << "jpegdec ! "

        // 轉換色彩空間以供 x264enc 使用
        << "videoconvert ! "

        // H.264 軟體編碼
        // tune=zerolatency : 最小化編碼延遲，配合接收端 latency=41
        // speed-preset=ultrafast : 最快速度，降低 CPU 使用
        // key-int-max : 控制 IDR 關鍵幀間隔
        << "x264enc"
        << " tune=zerolatency"
        << " speed-preset=ultrafast"
        << " bitrate=" << H264_BITRATE
        << " key-int-max=" << H264_KEY_INT << " ! "

        // 確保輸出 byte-stream 格式，h264parse 在接收端會處理格式轉換
        << "video/x-h264,stream-format=byte-stream ! "

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
