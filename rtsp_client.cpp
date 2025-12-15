#include <gst/gst.h>
#include <iostream>
#include "rtsp_config.h"

int main(int argc, char *argv[]) {
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;
    GMainLoop *loop;

    // 1. 初始化 GStreamer
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

    // 2. 建立 Pipeline
    // 使用 gst_parse_launch，語法和 gst-launch-1.0 指令相同
    // const char *pipeline_str = "rtspsrc location=rtsp://192.168.144.217:8554/test ! decodebin ! autovideosink";
    const char *pipeline_str = "rtspsrc location=" RTSP_URL " ! decodebin ! autovideosink";
    pipeline = gst_parse_launch(pipeline_str, NULL);

    if (!pipeline) {
        std::cerr << "Pipeline could not be created. Exiting." << std::endl;
        return -1;
    }

    // 3. 取得 Pipeline 的 Bus
    bus = gst_element_get_bus(pipeline);

    // 4. 將 Pipeline 設定為播放狀態
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "RTSP client pipeline is running..." << std::endl;

    // 5. 監聽 Bus 上的訊息 (錯誤、串流結束等)
    // 這會一直等待，直到出現 ERROR 或 EOS
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, 
                                     (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // 處理訊息
    if (msg != NULL) {
        GError *err = NULL;
        gchar *debug_info = NULL;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr << "Error received from element " << GST_OBJECT_NAME(msg->src) << ": " << err->message << std::endl;
                std::cerr << "Debugging information: " << (debug_info ? debug_info : "none") << std::endl;
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                std::cout << "End-Of-Stream reached." << std::endl;
                break;
            default:
                // 不應發生
                std::cerr << "Unexpected message received." << std::endl;
                break;
        }
        gst_message_unref(msg);
    }

    // 6. 清理資源
    std::cout << "Cleaning up and exiting." << std::endl;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(bus);
    g_main_loop_unref(loop);

    return 0;
}
