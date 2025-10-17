#include "Capture.h"
#include <iostream>
#include <cstdlib>
extern "C"
{
#include <libavdevice/avdevice.h>
}

Capture::Capture(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> queue)
    : raw_frame_queue_(queue) {}

Capture::~Capture()
{
    stop();
    if (dec_ctx_)
        avcodec_free_context(&dec_ctx_);
    if (in_fmt_ctx_)
        avformat_close_input(&in_fmt_ctx_);
}

bool Capture::start()
{
    avdevice_register_all();

    const AVInputFormat *ifmt = av_find_input_format("x11grab");
    AVDictionary *options = nullptr;
    av_dict_set(&options, "framerate", "60", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);

    // 从环境变量获取显示器名称，如果不存在则默认为 ":0.0"
    const char *display_env = getenv("DISPLAY");
    const char *display_name = (display_env) ? display_env : ":0.0";
    std::cout << "[Capture] Attempting to open display: " << display_name << std::endl;

    if (avformat_open_input(&in_fmt_ctx_, display_name, ifmt, &options) != 0)
    {
        std::cerr << "[Capture] ERROR: Cannot open x11grab device." << std::endl;
        return false;
    }
    av_dict_free(&options);

    if (avformat_find_stream_info(in_fmt_ctx_, nullptr) < 0)
    {
        std::cerr << "[Capture] ERROR: Cannot find stream info." << std::endl;
        return false;
    }

    video_stream_index_ = av_find_best_stream(in_fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0)
    {
        std::cerr << "[Capture] ERROR: Cannot find video stream." << std::endl;
        return false;
    }

    AVStream *stream = in_fmt_ctx_->streams[video_stream_index_];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    dec_ctx_ = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx_, stream->codecpar);
    if (avcodec_open2(dec_ctx_, dec, nullptr) < 0)
    {
        std::cerr << "[Capture] ERROR: Failed to open decoder." << std::endl;
        return false;
    }
    std::cout << "[Capture] Started successfully." << std::endl;
    return true;
}

void Capture::stop()
{
    stop_flag_ = true;
    raw_frame_queue_->stop();
}

void Capture::run()
{
    auto packet = make_av_packet();

    while (!stop_flag_)
    {
        if (av_read_frame(in_fmt_ctx_, packet.get()) >= 0)
        {
            if (packet->stream_index == video_stream_index_)
            {
                if (avcodec_send_packet(dec_ctx_, packet.get()) == 0)
                {
                    while (true)
                    {
                        auto frame = make_av_frame();
                        int ret = avcodec_receive_frame(dec_ctx_, frame.get());
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        {
                            break; // 需要更多数据或已到达流末尾，退出内层循环
                        }
                        else if (ret < 0)
                        {
                            // 真正的解码错误
                            break;
                        }
                        raw_frame_queue_->push(std::move(frame));
                    }
                }
            }
            av_packet_unref(packet.get());
        }
        else
        {
            break;
        }
    }

    // 清空解码器中剩余的帧
    avcodec_send_packet(dec_ctx_, nullptr);
    while (true)
    {
        auto frame = make_av_frame();
        int ret = avcodec_receive_frame(dec_ctx_, frame.get());
        if (ret == AVERROR_EOF || ret < 0)
        {
            break;
        }
        raw_frame_queue_->push(std::move(frame));
    }

    std::cout << "[Capture] Thread finished." << std::endl;
}