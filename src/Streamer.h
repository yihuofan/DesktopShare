#pragma once

#include "FFMpegWrappers.h"
#include "ThreadSafeQueue.h"
#include <string>
#include <thread>
#include <atomic>
#include <memory>

extern "C"
{
#include <libavformat/avformat.h>
}

class Streamer
{
public:
    Streamer(std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> queue);
    ~Streamer();

    bool start(const std::string &url, AVCodecContext *enc_ctx);
    void stop();
    void run();

private:
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> header_written_{false};
    std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_packet_queue_;

    struct AVFormatContextDeleter
    {
        void operator()(AVFormatContext *ctx) const
        {
            if (ctx)
            {
                avformat_free_context(ctx);
            }
        }
    };
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> out_fmt_ctx_;

    AVStream *out_stream_{nullptr};
    AVRational encoder_time_base_;
};