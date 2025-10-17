#include "Streamer.h"
#include <iostream>

Streamer::Streamer(std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> queue)
    : encoded_packet_queue_(queue) {}

Streamer::~Streamer()
{
    stop();
    if (out_fmt_ctx_)
    {
        if (header_written_)
        {
            av_write_trailer(out_fmt_ctx_.get());
        }
        if (!(out_fmt_ctx_->oformat->flags & AVFMT_NOFILE) && out_fmt_ctx_->pb)
        {
            avio_closep(&out_fmt_ctx_->pb);
        }
    }
}

bool Streamer::start(const std::string &url, AVCodecContext *enc_ctx)
{
    AVFormatContext *fmt_ctx_raw = nullptr;
    avformat_alloc_output_context2(&fmt_ctx_raw, nullptr, "rtsp", url.c_str());
    if (!fmt_ctx_raw)
    {
        std::cerr << "[Streamer] ERROR: Could not create output context." << std::endl;
        return false;
    }
    out_fmt_ctx_.reset(fmt_ctx_raw);

    out_stream_ = avformat_new_stream(out_fmt_ctx_.get(), nullptr);
    if (!out_stream_)
    {
        std::cerr << "[Streamer] ERROR: Failed to create new stream." << std::endl;
        return false;
    }

    if (avcodec_parameters_from_context(out_stream_->codecpar, enc_ctx) < 0)
    {
        std::cerr << "[Streamer] ERROR: Failed to copy codec parameters." << std::endl;
        return false;
    }
    out_stream_->codecpar->codec_tag = 0;

    if (!(out_fmt_ctx_->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&out_fmt_ctx_->pb, url.c_str(), AVIO_FLAG_WRITE) < 0)
        {
            std::cerr << "[Streamer] ERROR: Could not open output URL." << std::endl;
            return false;
        }
    }

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);

    if (avformat_write_header(out_fmt_ctx_.get(), &options) < 0)
    {
        std::cerr << "[Streamer] ERROR: Failed to write header." << std::endl;
        av_dict_free(&options); // 即使失败也要释放
        return false;
    }

    // 检查字典是否已被消耗，如果没有则释放它
    if (options)
    {
        av_dict_free(&options);
    }

    header_written_ = true;
    encoder_time_base_ = enc_ctx->time_base;

    std::cout << "[Streamer] Started successfully. Streaming to " << url << " via TCP." << std::endl;
    return true;
}

void Streamer::stop()
{
    stop_flag_ = true;
    if (encoded_packet_queue_)
    {
        encoded_packet_queue_->stop();
    }
}

void Streamer::run()
{
    AVPacketPtr packet;
    while (!stop_flag_ && encoded_packet_queue_->wait_and_pop(packet))
    {
        if (!packet)
        {
            continue;
        }

        packet->stream_index = out_stream_->index;
        av_packet_rescale_ts(packet.get(), encoder_time_base_, out_stream_->time_base);
        if (av_interleaved_write_frame(out_fmt_ctx_.get(), packet.get()) < 0)
        {
            std::cerr << "[Streamer] ERROR: Failed to write frame." << std::endl;
            break;
        }
    }

    std::cout << "[Streamer] Thread finished." << std::endl;
}