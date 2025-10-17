#include "Capture.h"
#include "Encoder.h"
#include "Streamer.h"
#include "ProcessManager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>

std::atomic_bool g_stop_flag = false;

void signal_handler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_stop_flag = true;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);

    // 启动 MediaMTX 服务器，并传递配置文件路径作为参数
    std::string mediamtx_path = "/home/user/workspace/desktopshare/Mediamttx/mediamtx";
    std::string config_path = "/home/user/workspace/desktopshare/Mediamttx/mediamtx.yml";
    ProcessManager mediamtx_manager(mediamtx_path, {config_path}); 
    if (!mediamtx_manager.start())
    {
        std::cerr << "FATAL: Could not start mediamtx server. Please ensure the 'mediamtx' executable is in a 'mediamtx' subdirectory." << std::endl;
        return 1; // 启动失败则直接退出
    }

    // 默认推流地址固定为本地
    std::string rtsp_url = "rtsp://localhost:8554/live";
    std::cout << "Target RTSP URL is: " << rtsp_url << std::endl;

    auto raw_frame_queue = std::make_shared<ThreadSafeQueue<AVFramePtr>>();
    auto encoded_packet_queue = std::make_shared<ThreadSafeQueue<AVPacketPtr>>();

    Capture capture(raw_frame_queue);
    if (!capture.start())
        return -1;

    Encoder encoder(raw_frame_queue, encoded_packet_queue);
    if (!encoder.start(1920, 1080))
        return -1;

    Streamer streamer(encoded_packet_queue);
    if (!streamer.start(rtsp_url, encoder.get_codec_context()))
        return -1;

    std::thread capture_thread(&Capture::run, &capture);
    std::thread encoder_thread(&Encoder::run, &encoder);
    std::thread streamer_thread(&Streamer::run, &streamer);

    while (!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Stopping all threads..." << std::endl;
    capture.stop();
    encoder.stop();
    streamer.stop();

    capture_thread.join();
    encoder_thread.join();
    streamer_thread.join();

    // 停止 MediaMTX 服务器
    mediamtx_manager.stop();

    std::cout << "Application finished." << std::endl;
    return 0;
}