#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <iostream>

int main(int argc, char *argv[]) {
    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;

    // 1. 初始化 GStreamer
    gst_init(&argc, &argv);

    // 建立主迴圈 (Main Loop)，這是 GLib/GStreamer 的核心
    loop = g_main_loop_new(NULL, FALSE);

    // 2. 建立 RTSP Server 物件
    server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, "8554"); // 設定 Port

    // 3. 取得掛載點 (Mount Points) 管理器
    mounts = gst_rtsp_server_get_mount_points(server);

    // 4. 建立 Media Factory (負責生成串流)
    factory = gst_rtsp_media_factory_new();

    // 設定 Pipeline
    // 注意：rtph264pay 的 name=pay0 是 RTSP Server 協定必須的
    gst_rtsp_media_factory_set_launch(factory, 
        "( v4l2src device=/dev/video0 ! videoconvert ! x264enc tune=zerolatency speed-preset=ultrafast ! rtph264pay name=pay0 pt=96 )");

    // 告知 Factory 這是共享的串流 (多人看同一個畫面，而不是每個人開一個新鏡頭)
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    // 5. 將 Factory 掛載到路徑 /test
    gst_rtsp_mount_points_add_factory(mounts, "/test", factory);

    // 清理物件引用計數 (mounts 已經加入 server，這裡可以 unref)
    g_object_unref(mounts);

    // 6. 啟動 Server
    gst_rtsp_server_attach(server, NULL);

    std::cout << "GStreamer RTSP Server is running (C++)..." << std::endl;
    std::cout << "Stream ready at rtsp://127.0.0.1:8554/test" << std::endl;

    // 進入主迴圈，程式會停在這裡直到被終止
    g_main_loop_run(loop);

    return 0;
}
