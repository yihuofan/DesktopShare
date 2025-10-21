# DesktopShare

一个轻量级的 Linux 桌面屏幕共享与直播推流工具。
---

## 版本演进

### V1：基础推流原型
- **功能**：捕获桌面 → 软件编码 → 推流至 **外部 MediaMTX RTSP 服务器**
- **架构**：纯后端，无网络服务模块
- **定位**：体验 FFmpeg 屏幕捕获与编码

### V2：独立 RTSP 服务器（当前版本）
- **核心改进**：
  - 移除对 MediaMTX 的依赖
  - 集成开源网络库 [`net`](https://github.com/PHZ76/net) 和 [`xop`](https://github.com/PHZ76/xop)（轻量级 RTSP/RTP 实现）
  - 程序自身作为 RTSP 服务器，支持客户端直接拉流（`rtsp://<ip>:<port>/live`）
- **数据流**：
  ```
  X11 桌面捕获 (x11grab)
      ↓
  FFmpeg 软件编码 (libx264)
      ↓
  自建 RTSP 服务 (net + xop)
      ↓
  客户端拉流播放
  ```

---

## 项目结构

```
RTSP_Server_V2/
├── CMakeLists.txt
└── src/
    ├── ffmpeg7.1/          # 静态链接的 FFmpeg 7.1（含头文件与库）
    ├── net/                # 网络基础库（TCP/UDP/EventLoop）
    ├── xop/                # RTSP/RTP 协议实现
    ├── Capture.{h,cpp}     # 屏幕捕获模块（基于 x11grab）
    ├── Encoder.{h,cpp}     # 视频编码封装
    ├── FFMpegWrappers.h    # FFmpeg C API 的 C++ 封装（RAII 资源管理）
    ├── RtspServerModule.{h,cpp}  # RTSP 服务集成与流发布
    ├── ThreadSafeQueue.h   # 线程安全队列（生产者-消费者模型）
    └── main.cpp            # 主入口
```

---

## 技术栈

| 组件 | 用途 |
|------|------|
| **FFmpeg 7.1** | 核心多媒体处理 |
| &nbsp;&nbsp;– `libavdevice` | X11 屏幕捕获 (`x11grab`) |
| &nbsp;&nbsp;– `libavcodec` | H.264 软件编码 (`libx264`) |
| &nbsp;&nbsp;– `libavformat` | 封装为 RTP/RTSP 流（辅助）|
| &nbsp;&nbsp;– `libswscale` | 像素格式转换（如 BGR → YUV420P）|
| **net + xop** | 自主 RTSP 服务（替代 MediaMTX）|
| **C++17** | 语言标准（RAII、智能指针、线程安全）|

> 💡 注：当前使用 **软件编码**，后续版本将引入硬件加速。

---

## 构建与运行

### 依赖项
- GCC/G++ (≥9.0)
- CMake (≥3.16)
- X11 开发库（`libx11-dev`, `libxext-dev`）
- FFmpeg 7.1（可网上寻找已编译版本）

---

## 迭代路线图

| 版本 | 目标 |
|------|------|
| **V3** | 性能优化：替换 `x11grab` 为 **原生 XCB/EGL/DRM** 屏幕捕获，降低延迟与 CPU 占用 |
| **V4** | 增加 **图形化界面**：基于 OpenGL + ImGui，提供启动/停止/状态监控 |
| **V5** | **多模态增强**：<br>• 音频采集（ALSA/PulseAudio）<br>• NVIDIA NVENC / Intel QSV / VAAPI 硬件编码<br>• 同时支持 RTSP 服务 + RTMP 推流<br>• UI 动态调节：码率、帧率、GOP、分辨率等 |
```