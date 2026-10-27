#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <glib.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include "rtsp_config.h"

// 串流參數（由 config.yaml 載入），預設值對齊 D80N 實機
struct StreamConfig {
    std::string codec = "h264";  // h264 / h265
    int width = 1920;            // 1080p
    int height = 1080;
    int fps = 30;                // 25 / 30
    int bitrate = 2048;          // kbps，512 ~ 2048
};

static std::string trim(const std::string &s) {
    const char *ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static bool parse_int(const std::string &s, int &out) {
    try {
        size_t pos = 0;
        out = std::stoi(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

// 簡易 YAML 讀取：只支援單層 "key: value"，# 之後視為註解
static bool read_yaml(const std::string &path, std::map<std::string, std::string> &kv) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
            value.back() == value.front()) {
            value = value.substr(1, value.size() - 2);
        }
        if (!key.empty()) kv[key] = value;
    }
    return true;
}

// 載入並驗證設定；設定檔不存在時使用預設值，數值不合法時回傳 false
static bool load_config(const std::string &path, StreamConfig &cfg) {
    std::map<std::string, std::string> kv;
    if (!read_yaml(path, kv)) {
        std::cout << "[Config] " << path << " not found, using defaults" << std::endl;
        return true;
    }
    std::cout << "[Config] loaded " << path << std::endl;

    for (const auto &[key, value] : kv) {
        if (key == "codec") {
            if (value != "h264" && value != "h265") {
                std::cerr << "[Config] codec must be h264 or h265, got '" << value << "'" << std::endl;
                return false;
            }
            cfg.codec = value;
        } else if (key == "resolution") {
            if (value == "1080p") {
                cfg.width = 1920;
                cfg.height = 1080;
            } else if (value == "720p") {
                cfg.width = 1280;
                cfg.height = 720;
            } else {
                std::cerr << "[Config] resolution must be 1080p or 720p, got '" << value << "'" << std::endl;
                return false;
            }
        } else if (key == "fps") {
            if (!parse_int(value, cfg.fps) || (cfg.fps != 25 && cfg.fps != 30)) {
                std::cerr << "[Config] fps must be 25 or 30, got '" << value << "'" << std::endl;
                return false;
            }
        } else if (key == "bitrate") {
            if (!parse_int(value, cfg.bitrate) || cfg.bitrate < 512 || cfg.bitrate > 2048) {
                std::cerr << "[Config] bitrate must be 512 ~ 2048 (kbps), got '" << value << "'" << std::endl;
                return false;
            }
        } else {
            std::cerr << "[Config] unknown key '" << key << "' ignored" << std::endl;
        }
    }
    return true;
}

// 未指定設定檔時：先找目前目錄，再找原始碼目錄（從 build/ 執行時適用）
static std::string resolve_config_path(int argc, char *argv[]) {
    if (argc > 1) return argv[1];
    if (std::ifstream(CONFIG_FILE_NAME)) return CONFIG_FILE_NAME;
#ifdef RTSP_SOURCE_DIR
    return std::string(RTSP_SOURCE_DIR) + "/" + CONFIG_FILE_NAME;
#else
    return CONFIG_FILE_NAME;
#endif
}

// 攝影機來源：以指定解析度擷取，解碼並轉成 raw video
static std::string build_source(int width, int height) {
    std::ostringstream oss;

#if defined(_WIN32)
    // Windows: 使用 mfvideosrc (Media Foundation)
    // 使用 JPEG 輸出：raw(YUY2) 在高解析度下幀率很低，
    // JPEG 格式才支援 30fps，避免相機降頻導致串流 freeze
    oss << "mfvideosrc device-index=" << VIDEO_DEVICE_INDEX << " do-timestamp=true ! "
        << "image/jpeg,width=" << width
        << ",height=" << height
        << ",framerate=" << CAM_FPS << "/1 ! "
        << "jpegdec";
#elif defined(__APPLE__)
    // macOS: 使用 avfvideosrc (AVFoundation)，直接輸出 raw video
    oss << "avfvideosrc device-index=" << VIDEO_DEVICE_INDEX << " ! "
        << "video/x-raw,width=" << width
        << ",height=" << height
        << ",framerate=" << CAM_FPS << "/1";
#else
    // Linux: 使用 v4l2src，攝影機輸出 MJPG 後解碼
    oss << "v4l2src device=" << VIDEO_DEVICE << " ! "
        << "image/jpeg,width=" << width
        << ",height=" << height
        << ",framerate=" << CAM_FPS << "/1 ! "
        << "jpegdec";
#endif

    return oss.str();
}

// 試開攝影機並等待 1 幀，確認此解析度可用（攝影機不支援時 caps 協商會失敗）
static bool probe_capture(int width, int height) {
    std::string desc = build_source(width, height) + " ! fakesink";
    GError *err = NULL;
    GstElement *pipe = gst_parse_launch(desc.c_str(), &err);
    if (!pipe) {
        if (err) g_error_free(err);
        return false;
    }

    // num-buffers=1：來源送出 1 幀後即發 EOS，收到 EOS 代表擷取成功
    GstIterator *it = gst_bin_iterate_sources(GST_BIN(pipe));
    GValue item = G_VALUE_INIT;
    if (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        g_object_set(g_value_get_object(&item), "num-buffers", 1, NULL);
        g_value_unset(&item);
    }
    gst_iterator_free(it);

    gst_element_set_state(pipe, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipe);
    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, 5 * GST_SECOND, (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    bool ok = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
    if (msg) gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(pipe);
    return ok;
}

// 決定攝影機擷取解析度：優先使用輸出解析度，攝影機不支援時退回 720p 再放大
static bool select_capture(const StreamConfig &cfg, int &cap_width, int &cap_height) {
    const int candidates[][2] = {{cfg.width, cfg.height}, {1280, 720}};
    for (const auto &c : candidates) {
        if (c[0] > cfg.width) continue;
        std::cout << "[Camera] probing " << c[0] << "x" << c[1] << "@" << CAM_FPS << "fps ..." << std::endl;
        if (probe_capture(c[0], c[1])) {
            cap_width = c[0];
            cap_height = c[1];
            return true;
        }
    }
    return false;
}

// 建立與接收端相容的 pipeline 字串
// 接收端 (skai-edge) 使用: rtspsrc ! rtp{h264,h265}depay ! {h264,h265}parse ! nvv4l2decoder ! nvvidconv
// codec 由 DESCRIBE 回應的 SDP rtpmap 判斷，因此 server 需輸出標準 H.264 / H.265 RTP 串流
static std::string build_pipeline(const StreamConfig &cfg, int cap_width, int cap_height) {
    std::ostringstream oss;
    oss << "( " << build_source(cap_width, cap_height) << " ! ";

    // 攝影機解析度不足時以 CPU (videoscale) 放大到輸出解析度；
    // 放在 videoconvert 之前，直接縮放 jpegdec 輸出的 I420，省去多一次色彩轉換
    if (cap_width != cfg.width || cap_height != cfg.height) {
        oss << "videoscale ! video/x-raw,width=" << cfg.width << ",height=" << cfg.height << " ! ";
    }
    oss << "videoconvert ! ";

    // 多數 webcam 只提供 30fps，以 videorate 轉成設定的幀率（30fps 時直接通過）
    oss << "videorate ! video/x-raw,framerate=" << cfg.fps << "/1 ! ";

    // 共用部分：H.264 / H.265 編碼 + RTP 封包
    //
    // tune 可選值（未設定則使用預設，啟用 lookahead + B-frame，畫質較佳但延遲較高）:
    //   stillimage  : 靜態圖片優化，增加壓縮延遲，不適合串流
    //   animation   : 卡通/平滑色塊動畫優化，對延遲無特別影響
    //   grain       : 保留底片顆粒感，對延遲無特別影響
    //   psnr        : 最大化 PSNR 指標（測試用），增加延遲
    //   ssim        : 最大化 SSIM 指標（測試用），增加延遲
    //   fastdecode  : 降低接收端解碼 CPU 用量，對編碼延遲無影響
    //   zerolatency : 關閉 lookahead 與 B-frame，每幀立即輸出，延遲最低（D80N 實機也沒有 B-frame）
    //
    // speed-preset 可選值（相對速度以 medium=1x 為基準；壓縮效率為同畫質下相對 veryslow 的額外 bitrate 開銷）:
    //   ultrafast : ~16x，壓縮效率 -50%（x264 會關閉 CABAC / 8x8 transform，退成 Constrained Baseline）
    //   superfast : ~10x，壓縮效率 -35%
    //   veryfast  :  ~6x，壓縮效率 -22%（H.264 使用：1 個參考幀，對齊 D80 實機）
    //   faster    : ~2.5x，壓縮效率 -18%（2 個參考幀）
    //   fast      : ~1.7x，壓縮效率 -14%
    //   medium    :    1x，壓縮效率 -10%（預設值）
    //   slow      : ~0.5x，壓縮效率  -5%
    //   slower    : ~0.25x，壓縮效率 -3%
    //   veryslow  : ~0.13x，壓縮效率  0%（最佳基準）
    // key-int-max : 控制 IDR 關鍵幀間隔
    // bitrate     : 單位 kbps（x264enc 與 x265enc 相同）
    if (cfg.codec == "h265") {
        // x265 比 x264 慢數倍，即時編碼使用 ultrafast（x265 不會因此降 profile，仍為 Main）
        oss << "x265enc"
            << " tune=zerolatency"
            << " speed-preset=ultrafast"
            << " bitrate=" << cfg.bitrate
            << " key-int-max=" << VIDEO_KEY_INT << " ! "
            << "h265parse ! "
            // pt=98 : 對齊 D80 實機（H.264 用 96，H.265 用 98）
            << "rtph265pay name=pay0 pt=98 config-interval=-1"
            << " )";
    } else {
        oss << "x264enc"
            << " tune=zerolatency"
            << " speed-preset=veryfast"
            << " bitrate=" << cfg.bitrate
            << " key-int-max=" << VIDEO_KEY_INT << " ! "

            // 指定 High profile，對齊 D80N 實機
            << "video/x-h264,profile=high ! "

            // h264parse : 正規化 H.264 bitstream，確保 NAL 對齊與 timestamp 正確，
            //             對 nvv4l2decoder 等硬體解碼器的相容性較好
            << "h264parse ! "

            // RTP 封包
            // config-interval=-1 : 每個 IDR frame 都附帶 SPS/PPS，
            //                       讓接收端隨時加入都能正確解碼
            // pt=96              : 動態 payload type，標準 H.264 慣例
            << "rtph264pay name=pay0 pt=96 config-interval=-1"
            << " )";
    }

    return oss.str();
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    // 用法: rtsp_server [config.yaml 路徑]
    StreamConfig cfg;
    if (!load_config(resolve_config_path(argc, argv), cfg)) {
        return 1;
    }
    std::cout << "[Config] codec=" << cfg.codec
              << " resolution=" << cfg.width << "x" << cfg.height
              << " fps=" << cfg.fps
              << " bitrate=" << cfg.bitrate << "kbps" << std::endl;

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // 建立 RTSP Server
    GstRTSPServer *server = gst_rtsp_server_new();
    gst_rtsp_server_set_address(server, RTSP_HOST);
    gst_rtsp_server_set_service(server, RTSP_PORT);

    // 建立 Media Factory
    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    int cap_width = 0, cap_height = 0;
    if (!select_capture(cfg, cap_width, cap_height)) {
        std::cerr << "[Camera] cannot capture at " << cfg.width << "x" << cfg.height
                  << " or 1280x720 @" << CAM_FPS << "fps" << std::endl;
        return 1;
    }
    if (cap_width != cfg.width || cap_height != cfg.height) {
        std::cout << "[Camera] capture " << cap_width << "x" << cap_height
                  << ", upscale to " << cfg.width << "x" << cfg.height << std::endl;
    }

    std::string pipeline = build_pipeline(cfg, cap_width, cap_height);
    std::cout << "[Pipeline] " << pipeline << std::endl;

    gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());

    // shared=TRUE：所有連線的 client 共用同一路串流，不重複開啟攝影機
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    gst_rtsp_mount_points_add_factory(mounts, RTSP_PATH, factory);
    g_object_unref(mounts);

    // 綁定失敗最常見的原因是 Linux 上非 root 使用 554 port
    if (gst_rtsp_server_attach(server, NULL) == 0) {
        std::cerr << "Failed to bind " << RTSP_HOST << ":" << RTSP_PORT
                  << " (port in use, or port < 1024 without root / CAP_NET_BIND_SERVICE)" << std::endl;
        g_object_unref(server);
        g_main_loop_unref(loop);
        return 1;
    }

    std::cout << "RTSP Server running at " << RTSP_URL << std::endl;

    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(server);
    return 0;
}
