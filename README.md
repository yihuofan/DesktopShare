# DesktopShare V1

当前版本是一个在Linux上运行的、低性能的、纯后端的屏幕直播推流工具。作为后续项目的起点。

# DesktopShare V2

改进为一个**独立的 RTSP 服务器**。捕获 Linux 桌面屏幕，使用 FFmpeg 进行 H.264 编码，并通过集成的 `net` 和 `xop` 网络库将视频流作为 RTSP 源提供服务，供客户端直接拉流观看。

此版本是项目迭代的第二阶段，核心改进是集成了外部网络库 (`net` 和 `xop`)，取代了第一版依赖mediamttx。

## 流程
V1: 捕获Linux桌面 (使用 FFmpeg x11grab) -> H.264软件编码 (使用 FFmpeg libx264) -> 推流到 MediaMTX RTSP服务器。

V2: 捕获Linux桌面 (使用 FFmpeg x11grab) -> H.264软件编码 (使用 FFmpeg libx264) -> 集成 net 和 xop 库，作为独立的 RTSP 服务器 -> 等待客户端连接并拉取流。

## 技术栈

  * **核心库**: **FFmpeg7.1**
      * `libavdevice`: 用于屏幕捕获。
      * `libavcodec`: 负责视频的编码和解码。
      * `libavformat`: 用于 RTSP 协议的封装与推流。
      * `libswscale`: 用于图像的缩放和像素格式转换。

## 如何构建和运行

### 依赖项

确保已安装以下依赖：

  * GCC/G++
  * CMake
  * FFmpeg 开发库（我用的已编译的linux上的ffmpeg）
```


## 迭代计划

### 第二版：移除对外部RTSP服务器的依赖，集成网络库，让程序自身成为rtsp服务器。

### 第三版：提高性能，替换为原生屏幕捕获等。

### 第四版：尝试 OpenGL与ImGui 增加图形化界面

### 第五版：增加音频采集和编码，支持NVIDIA硬件编码器，多协议，同时支持RTSP服务和RTMP推流，在UI中提供对码率、帧率、GOP等核心编码参数的动态调整。