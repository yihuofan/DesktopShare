#include "RtspServerModule.h"
#include "xop/H264Source.h" // 需要 H264Source 来创建视频源
#include <iostream>

RtspServerModule::RtspServerModule(std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_packet_queue)
    : encoded_packet_queue_(encoded_packet_queue),
      event_loop_(new xop::EventLoop()) // 创建 xop 事件循环
{
}

RtspServerModule::~RtspServerModule()
{
    stop();
    // 智能指针会自动管理 event_loop_ 和 rtsp_server_ 的生命周期
}

bool RtspServerModule::start(uint16_t port, const std::string &suffix, AVCodecContext *video_codec_ctx)
{
    if (is_running_)
    {
        std::cout << "[RtspServer] Server is already running." << std::endl;
        return true;
    }

    // 1. 创建 RtspServer 实例
    rtsp_server_ = xop::RtspServer::Create(event_loop_.get());
    if (!rtsp_server_)
    {
        std::cerr << "[RtspServer] ERROR: Failed to create RtspServer." << std::endl;
        return false;
    }

    // 2. 启动 RTSP 服务器，监听指定端口
    // 使用 "0.0.0.0" 监听所有网络接口
    if (!rtsp_server_->Start("0.0.0.0", port))
    {
        std::cerr << "[RtspServer] ERROR: Failed to start RTSP Server on port " << port << "." << std::endl;
        // 不需要手动停止，因为智能指针会在作用域结束时处理
        return false;
    }

    // 3. 创建媒体会话 (MediaSession)
    xop::MediaSession *session = xop::MediaSession::CreateNew(suffix);
    if (!session)
    {
        std::cerr << "[RtspServer] ERROR: Failed to create MediaSession." << std::endl;
        return false;
    }
    // 添加 H.264 视频源到通道 0
    session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
    // 可以在这里添加 AAC 音频源到通道 1 (如果后续实现了音频)
    // session->AddSource(xop::channel_1, xop::AACSource::CreateNew(samplerate, channels, false));

    // 设置连接和断开连接的回调，用于打印日志
    session->AddNotifyConnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port)
                                        { std::cout << "[RtspServer] Client connected: " << peer_ip << ":" << peer_port << std::endl; });
    session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port)
                                           { std::cout << "[RtspServer] Client disconnected: " << peer_ip << ":" << peer_port << std::endl; });

    // 4. 将媒体会话添加到 RTSP 服务器
    media_session_id_ = rtsp_server_->AddSession(session);
    if (media_session_id_ == 0)
    {
        std::cerr << "[RtspServer] ERROR: Failed to add MediaSession to server." << std::endl;
        // 如果添加失败，可能需要手动移除 session? xop 的文档可能需要查阅
        // 通常 rtsp_server_ 析构时会清理 session
        return false;
    }

    is_running_ = true;

    // 5. 启动网络事件循环线程和数据分发线程
    try
    {
        event_loop_thread_ = std::make_unique<std::thread>(&RtspServerModule::run_event_loop, this);
        dispatcher_thread_ = std::make_unique<std::thread>(&RtspServerModule::run_frame_dispatcher, this);
    }
    catch (const std::system_error &e)
    {
        std::cerr << "[RtspServer] ERROR: Failed to start threads: " << e.what() << std::endl;
        is_running_ = false;
        // 可能需要停止已启动的服务器部分
        if (rtsp_server_)
            rtsp_server_->RemoveSession(media_session_id_);
        return false;
    }

    std::cout << "[RtspServer] Server started successfully at rtsp://<your-ip>:" << port << "/" << suffix << std::endl;

    // 保存编码器的时间基，以备后用 (当前未使用，但保留)
    video_encoder_time_base_ = video_codec_ctx->time_base;

    return true;
}

void RtspServerModule::stop()
{
    // 使用 exchange 原子地设置标志并获取旧值，防止多次调用 stop
    if (is_running_.exchange(false))
    {
        std::cout << "[RtspServer] Stopping server..." << std::endl;

        // 1. 停止数据分发线程
        encoded_packet_queue_->stop(); // 通知队列停止，唤醒等待的线程
        if (dispatcher_thread_ && dispatcher_thread_->joinable())
        {
            dispatcher_thread_->join();
        }
        std::cout << "[RtspServer] Dispatcher thread stopped." << std::endl;

        // 2. 停止网络事件循环
        if (event_loop_)
        {
            event_loop_->Quit(); // 请求事件循环退出
        }
        if (event_loop_thread_ && event_loop_thread_->joinable())
        {
            event_loop_thread_->join();
        }
        std::cout << "[RtspServer] Event loop thread stopped." << std::endl;

        // 3. 停止 RTSP 服务器本身 (xop 库会自动处理资源释放)
        // 智能指针 rtsp_server_ 在析构时会自动调用其 Stop 方法或类似清理逻辑
        // 如果 xop::RtspServer 没有在析构时自动 Stop，则需要手动调用
        // if (rtsp_server_) {
        //     rtsp_server_->Stop();
        // }
        rtsp_server_ = nullptr; // 释放对服务器对象的引用

        std::cout << "[RtspServer] Server stopped completely." << std::endl;
    }
}

// 网络事件循环线程函数
void RtspServerModule::run_event_loop()
{
    std::cout << "[RtspServer] Event loop thread started." << std::endl;
    if (event_loop_)
    {
        event_loop_->Loop(); // 阻塞运行事件循环，直到 quit() 被调用
    }
    std::cout << "[RtspServer] Event loop thread finished." << std::endl;
}

// 帧数据分发线程函数
void RtspServerModule::run_frame_dispatcher()
{
    std::cout << "[RtspServer] Frame dispatcher thread started." << std::endl;
    while (is_running_)
    {
        AVPacketPtr packet;
        // 阻塞等待从编码队列中获取数据包
        if (!encoded_packet_queue_->wait_and_pop(packet))
        {
            // 如果返回 false，表示是由于 stop() 被调用且队列为空
            if (!is_running_)
            {
                break; // 确认是停止信号，退出循环
            }
            continue; // 否则，可能是虚假唤醒，继续等待
        }

        if (!packet)
            continue; // 健壮性检查

        // 假设视频流在通道 0
        if (packet->stream_index == 0 && rtsp_server_ && media_session_id_ != 0)
        {
            // 将 AVPacket 转换为 xop::AVFrame
            // 注意：xop::AVFrame 需要不包含 H.264 起始码 (00 00 00 01)
            // FFmpeg 编码器输出的 packet 通常是包含起始码的 Annex B 格式
            // H264Source 内部可能会处理，或者我们需要在这里处理

            // 简单的处理方式：假设 H264Source 能处理 Annex B
            xop::AVFrame video_frame(packet->size);
            if (video_frame.buffer)
            { // 检查内存是否分配成功
                video_frame.size = packet->size;

                // 判断是否是关键帧 (I帧)
                // SPS/PPS 通常和 I 帧一起发送，xop::H264Source 会处理 SDP
                bool is_key_frame = (packet->flags & AV_PKT_FLAG_KEY);
                video_frame.type = is_key_frame ? xop::VIDEO_FRAME_I : xop::VIDEO_FRAME_P;

                // 设置时间戳 - 使用 H264Source 提供的函数生成基于时钟的时间戳
                video_frame.timestamp = xop::H264Source::GetTimestamp();

                // 拷贝数据
                memcpy(video_frame.buffer.get(), packet->data, packet->size);

                // 推送帧数据到 RTSP 服务器
                rtsp_server_->PushFrame(media_session_id_, xop::channel_0, video_frame);
            }
            else
            {
                std::cerr << "[Dispatcher] Failed to allocate memory for xop::AVFrame." << std::endl;
            }
        }
        // 如果有音频流，在这里处理 packet->stream_index == 1 的情况
    }
    std::cout << "[RtspServer] Frame dispatcher thread finished." << std::endl;
}