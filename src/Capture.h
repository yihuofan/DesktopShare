#pragma once

#include "FFMpegWrappers.h"
#include "ThreadSafeQueue.h"
#include <thread>
#include <atomic>

// 前向声明，避免循环包含
struct AVFormatContext;
struct AVCodecContext;

class Capture
{
public:
    Capture(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> queue);
    ~Capture();
    bool start();
    void stop();
    void run();

private:
    std::shared_ptr<ThreadSafeQueue<AVFramePtr>> raw_frame_queue_;
    std::atomic_bool stop_flag_{false};

    // 使用原始指针，但生命周期由智能指针在析构函数中管理（或者手动管理）
    // 或者使用智能指针，但要小心所有权传递和生命周期问题
    AVFormatContext *in_fmt_ctx_ = nullptr;       // 用 reset 管理
    AVCodecContext *dec_ctx_ = nullptr;           // 用 reset 管理
    AVFormatContextPtr in_fmt_ctx_ptr_ = nullptr; // 使用智能指针管理输入上下文
    AVCodecContextPtr dec_ctx_ptr_ = nullptr;     // 使用智能指针管理解码上下文

    int video_stream_index_ = -1;
};