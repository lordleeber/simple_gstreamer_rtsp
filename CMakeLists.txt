cmake_minimum_required(VERSION 3.10)
project(SimpleRtspServer)

# 設定 C++ 標準 (建議使用 C++17 或更高)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 PkgConfig 工具，用來找 GStreamer
find_package(PkgConfig REQUIRED)

# 檢查並載入 GStreamer 及其 RTSP Server 模組
# 這裡對應指令列的: pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtsp-server-1.0
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_RTSP REQUIRED gstreamer-rtsp-server-1.0)

# 定義執行檔名稱 (rtsp_server) 和來源檔案 (rtsp_server.cpp)
add_executable(rtsp_server rtsp_server.cpp)

# 加入標頭檔路徑 (Include Directories)
target_include_directories(rtsp_server PRIVATE 
    ${GST_INCLUDE_DIRS}
    ${GST_RTSP_INCLUDE_DIRS}
)

# 加入連結庫 (Link Libraries)
target_link_libraries(rtsp_server PRIVATE 
    ${GST_LIBRARIES}
    ${GST_RTSP_LIBRARIES}
)

# --- RTSP Client ---
# 定義客戶端執行檔 (rtsp_client) 和來源檔案 (rtsp_client.cpp)
add_executable(rtsp_client rtsp_client.cpp)

# 為客戶端加入標頭檔路徑
target_include_directories(rtsp_client PRIVATE 
    ${GST_INCLUDE_DIRS}
)

# 為客戶端加入連結庫
target_link_libraries(rtsp_client PRIVATE 
    ${GST_LIBRARIES}
)

