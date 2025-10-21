cmake_minimum_required(VERSION 3.10)
project(RTSP_Server_V2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 添加 -Wno-deprecated-declarations 以兼容 xop 库中可能存在的旧式 API 用法
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")


find_package(PkgConfig REQUIRED)

# 查找FFmpeg库 (注意：此阶段理论上不再需要 libavdevice, 但保留 avformat 以防万一)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

# 收集所有 net 和 xop 的源文件
file(GLOB_RECURSE XOP_SOURCES "src/xop/*.cpp")
file(GLOB_RECURSE NET_SOURCES "src/net/*.cpp")

add_executable(rtsp_server
    src/main.cpp
    src/Capture.cpp
    src/Encoder.cpp
    src/RtspServerModule.cpp  # 使用新的模块
    ${XOP_SOURCES}
    ${NET_SOURCES}
)

target_include_directories(rtsp_server PRIVATE
    src/ # 允许包含 net/ 和 xop/ 下的头文件
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
)

target_link_libraries(rtsp_server PRIVATE
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    pthread
    avdevice
)