#pragma once

#include "FFMpegWrappers.h"
#include "ThreadSafeQueue.h"
#include <thread>
#include <atomic>

class Encoder
{
public:
    Encoder(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> raw_q,
            std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_q);
    ~Encoder();

    bool start(int width, int height);
    void stop();
    void run();

    AVCodecContext *get_codec_context() { return enc_ctx_.get(); }

private:
    std::shared_ptr<ThreadSafeQueue<AVFramePtr>> raw_frame_queue_;
    std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_packet_queue_;
    std::atomic_bool stop_flag_{false};

    AVCodecContextPtr enc_ctx_ = nullptr;
    SwsContextPtr sws_ctx_ = nullptr;
    AVFramePtr scaled_frame_ = nullptr;
    long frame_count_ = 0; // 用于设置 PTS
};