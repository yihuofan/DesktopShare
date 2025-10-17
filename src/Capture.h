#pragma once

#include "FFMpegWrappers.h"
#include "ThreadSafeQueue.h"
#include <thread>
#include <atomic>

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

    AVFormatContext *in_fmt_ctx_ = nullptr;
    AVCodecContext *dec_ctx_ = nullptr;
    int video_stream_index_ = -1;
};