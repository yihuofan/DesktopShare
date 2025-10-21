#pragma once

#include "FFMpegWrappers.h"
#include "ThreadSafeQueue.h"
#include "xop/RtspServer.h" // 包含 xop 库的头文件
#include "xop/MediaSession.h"
#include <thread>
#include <atomic>
#include <string>

class RtspServerModule
{
public:
    // 构造函数接收编码后的数据包队列
    RtspServerModule(std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_packet_queue);
    ~RtspServerModule();

    // 启动服务器，需要编码器上下文来获取流信息
    bool start(uint16_t port, const std::string &suffix, AVCodecContext *video_codec_ctx);

    // 停止服务器
    void stop();

private:
    // 网络事件循环线程函数
    void run_event_loop();
    // 帧数据分发线程函数
    void run_frame_dispatcher();

    std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_packet_queue_;

    std::unique_ptr<xop::EventLoop> event_loop_;   // xop 的事件循环
    std::shared_ptr<xop::RtspServer> rtsp_server_; // xop 的 RTSP 服务器实例
    xop::MediaSessionId media_session_id_ = 0;     // 媒体会话 ID

    std::unique_ptr<std::thread> event_loop_thread_; // 网络事件循环线程
    std::unique_ptr<std::thread> dispatcher_thread_; // 数据分发线程
    std::atomic_bool is_running_{false};             // 运行状态标志

    AVRational video_encoder_time_base_; // 保存编码器时间基，用于日志或调试
};