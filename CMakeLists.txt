cmake_minimum_required(VERSION 3.10)
project(DesktopShare_V1)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# FFmpeg 自定义安装路径
set(FFMPEG_DIR "/home/user/workspace/desktopshare/ffmpeg7.1")

# 头文件与库文件路径
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_DIR}/include")
set(FFMPEG_LIB_DIRS "${FFMPEG_DIR}/lib")

# 查找各个 FFmpeg 库
find_library(AVFORMAT_LIB avformat PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(AVCODEC_LIB avcodec   PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(AVUTIL_LIB   avutil    PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(AVDEVICE_LIB avdevice  PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(SWSCALE_LIB  swscale   PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(SWRESAMPLE_LIB swresample PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)
find_library(AVFILTER_LIB   avfilter   PATHS ${FFMPEG_LIB_DIRS} NO_DEFAULT_PATH)

# 创建可执行目录
add_executable(DesktopShare_V1
    src/main.cpp
    src/Capture.cpp
    src/Encoder.cpp
    src/Streamer.cpp
    src/ProcessManager.cpp
)

# 包含头文件目录
target_include_directories(DesktopShare_V1 PRIVATE ${FFMPEG_INCLUDE_DIRS})

# 链接库
target_link_libraries(DesktopShare_V1 PRIVATE
    ${AVFORMAT_LIB}
    ${AVCODEC_LIB}
    ${AVUTIL_LIB}
    ${AVDEVICE_LIB}
    ${SWSCALE_LIB}
    ${SWRESAMPLE_LIB} 
    ${AVFILTER_LIB}
    pthread
    X11
)

# 设置将库路径嵌入到可执行文件中
# 为构建目录 (BUILD_RPATH) 和安装目录 (INSTALL_RPATH) 设置运行时路径
set_target_properties(DesktopShare_V1 PROPERTIES
    BUILD_RPATH "${FFMPEG_LIB_DIRS}"
    INSTALL_RPATH "${FFMPEG_LIB_DIRS}"
)