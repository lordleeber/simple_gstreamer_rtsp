cmake_minimum_required(VERSION 3.10)
project(SimpleRtspServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 平台偵測：Windows vs Linux
# ============================================================
if(WIN32)
    # Windows: 從環境變數或預設路徑找 GStreamer MinGW 版本
    if(DEFINED ENV{GSTREAMER_1_0_ROOT_MINGW_X86_64})
        set(GSTREAMER_ROOT "$ENV{GSTREAMER_1_0_ROOT_MINGW_X86_64}")
    else()
        set(GSTREAMER_ROOT "C:/Program Files/gstreamer/1.0/mingw_x86_64"
            CACHE PATH "GStreamer MinGW installation root")
    endif()

    # 路徑含空格時 MinGW 連結器會截斷，轉成 8.3 短路徑避免此問題
    execute_process(
        COMMAND powershell -NoProfile -Command
            "$fso = New-Object -ComObject Scripting.FileSystemObject; $fso.GetFolder('${GSTREAMER_ROOT}').ShortPath"
        OUTPUT_VARIABLE GSTREAMER_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    file(TO_CMAKE_PATH "${GSTREAMER_ROOT}" GSTREAMER_ROOT)

    message(STATUS "Platform: Windows (MinGW)")
    message(STATUS "GStreamer root (short path): ${GSTREAMER_ROOT}")

    set(GST_INCLUDE_DIRS
        "${GSTREAMER_ROOT}/include/gstreamer-1.0"
        "${GSTREAMER_ROOT}/include/glib-2.0"
        "${GSTREAMER_ROOT}/lib/glib-2.0/include"
    )
    set(GST_LIBRARIES
        "${GSTREAMER_ROOT}/lib/libgstreamer-1.0.dll.a"
        "${GSTREAMER_ROOT}/lib/libglib-2.0.dll.a"
        "${GSTREAMER_ROOT}/lib/libgobject-2.0.dll.a"
    )
    set(GST_RTSP_INCLUDE_DIRS ${GST_INCLUDE_DIRS})
    set(GST_RTSP_LIBRARIES
        "${GSTREAMER_ROOT}/lib/libgstrtspserver-1.0.dll.a"
    )
else()
    # Linux: 使用 PkgConfig
    message(STATUS "Platform: Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED gstreamer-1.0)
    pkg_check_modules(GST_RTSP REQUIRED gstreamer-rtsp-server-1.0)
endif()

# ============================================================
# 編譯目標
# ============================================================

# --- RTSP Server ---
add_executable(rtsp_server rtsp_server.cpp)
target_include_directories(rtsp_server PRIVATE
    ${GST_INCLUDE_DIRS}
    ${GST_RTSP_INCLUDE_DIRS}
)
target_link_libraries(rtsp_server PRIVATE
    ${GST_LIBRARIES}
    ${GST_RTSP_LIBRARIES}
)

# --- RTSP Client ---
add_executable(rtsp_client rtsp_client.cpp)
target_include_directories(rtsp_client PRIVATE
    ${GST_INCLUDE_DIRS}
)
target_link_libraries(rtsp_client PRIVATE
    ${GST_LIBRARIES}
)
