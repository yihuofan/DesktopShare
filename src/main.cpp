#include "Capture.h"
#include "Encoder.h"
#include "RtspServerModule.h" // 替换 Streamer.h
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread> // 需要包含 <thread>

std::atomic_bool g_stop_flag = false;

void signal_handler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_stop_flag = true;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler); // 注册 Ctrl+C 信号处理

    // RTSP 服务器配置
    const uint16_t rtsp_port = 8554;
    const std::string rtsp_suffix = "live";
    const int capture_width = 1920;
    const int capture_height = 1080;

    // 1. 创建共享队列
    auto raw_frame_queue = std::make_shared<ThreadSafeQueue<AVFramePtr>>();
    auto encoded_packet_queue = std::make_shared<ThreadSafeQueue<AVPacketPtr>>();

    // 2. 创建并初始化模块
    Capture capture_module(raw_frame_queue);
    if (!capture_module.start())
    {
        std::cerr << "Failed to start Capture module." << std::endl;
        return -1;
    }

    Encoder encoder_module(raw_frame_queue, encoded_packet_queue);
    if (!encoder_module.start(capture_width, capture_height))
    {
        std::cerr << "Failed to start Encoder module." << std::endl;
        return -1;
    }

    // 获取编码器上下文，确保编码器已初始化
    AVCodecContext *encoder_ctx = encoder_module.get_codec_context();
    if (!encoder_ctx)
    {
        std::cerr << "Failed to get encoder context after starting encoder." << std::endl;
        // 可能需要添加更好的同步机制，而不是依赖 start() 成功就认为 context 有效
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 简单等待
        encoder_ctx = encoder_module.get_codec_context();
        if (!encoder_ctx)
            return -1;
    }

    RtspServerModule rtsp_server_module(encoded_packet_queue);
    // 启动 RTSP 服务器模块，传入必要的参数
    if (!rtsp_server_module.start(rtsp_port, rtsp_suffix, encoder_ctx))
    {
        std::cerr << "Failed to start RtspServer module." << std::endl;
        return -1;
    }

    // 3. 启动工作线程
    std::thread capture_thread(&Capture::run, &capture_module);
    std::thread encoder_thread(&Encoder::run, &encoder_module);
    // RtspServerModule 内部已经启动了它自己的线程 (网络线程和分发线程)

    std::cout << "Application is running. Press Ctrl+C to stop." << std::endl;

    // 4. 等待停止信号
    while (!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 主线程休眠，避免空转
    }

    // 5. 收到停止信号，开始清理
    std::cout << "Stopping all modules..." << std::endl;
    // 按照依赖反向顺序停止：先停止接收数据的，再停止发送数据的
    rtsp_server_module.stop(); // 停止服务器会停止其内部线程
    encoder_module.stop();     // 停止编码器会停止从 raw_queue 取数据
    capture_module.stop();     // 停止采集器会停止向 raw_queue 放数据

    // 6. 等待工作线程结束
    if (capture_thread.joinable())
        capture_thread.join();
    if (encoder_thread.joinable())
        encoder_thread.join();
    // RtspServerModule 的线程在其 stop() 方法内部已经被 join

    std::cout << "Application finished." << std::endl;
    return 0;
}