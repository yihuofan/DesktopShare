#include "Capture.h"
#include <iostream>
#include <cstdlib> // 为了 getenv
extern "C"
{
#include <libavdevice/avdevice.h>
}

Capture::Capture(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> queue)
    : raw_frame_queue_(queue) {}

Capture::~Capture()
{
    stop();
}

bool Capture::start()
{
    avdevice_register_all();

    AVInputFormat *ifmt = av_find_input_format("x11grab");
    if (!ifmt)
    {
        std::cerr << "[Capture] ERROR: Cannot find input format 'x11grab'." << std::endl;
        return false;
    }

    AVDictionary *options = nullptr;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);
    // 尝试添加 draw_mouse=0 来隐藏鼠标指针
    av_dict_set(&options, "draw_mouse", "0", 0);

    AVFormatContext *fmt_ctx_raw = nullptr;
    const char *display_name = getenv("DISPLAY");
    if (!display_name)
    {
        std::cerr << "[Capture] ERROR: DISPLAY environment variable not set." << std::endl;
        av_dict_free(&options);
        return false;
    }

    if (avformat_open_input(&fmt_ctx_raw, display_name, ifmt, &options) != 0)
    {
        std::cerr << "[Capture] ERROR: Cannot open x11grab device '" << display_name << "'." << std::endl;
        av_dict_free(&options); // 即使失败也要释放字典
        return false;
    }
    in_fmt_ctx_ptr_.reset(fmt_ctx_raw); // 将原始指针交给智能指针管理
    av_dict_free(&options);             // 打开成功后也要释放字典

    if (avformat_find_stream_info(in_fmt_ctx_ptr_.get(), nullptr) < 0)
    {
        std::cerr << "[Capture] ERROR: Cannot find stream info." << std::endl;
        return false; // 智能指针会自动调用 Deleter 清理已打开的上下文
    }

    video_stream_index_ = av_find_best_stream(in_fmt_ctx_ptr_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0)
    {
        std::cerr << "[Capture] ERROR: Cannot find video stream." << std::endl;
        return false;
    }

    AVStream *stream = in_fmt_ctx_ptr_->streams[video_stream_index_];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!dec)
    {
        std::cerr << "[Capture] ERROR: Failed to find decoder for stream #" << video_stream_index_ << std::endl;
        return false;
    }

    AVCodecContext *dec_ctx_raw = avcodec_alloc_context3(dec);
    if (!dec_ctx_raw)
    {
        std::cerr << "[Capture] ERROR: Failed to allocate decoder context." << std::endl;
        return false;
    }
    dec_ctx_ptr_.reset(dec_ctx_raw); // 交给智能指针

    if (avcodec_parameters_to_context(dec_ctx_ptr_.get(), stream->codecpar) < 0)
    {
        std::cerr << "[Capture] ERROR: Failed to copy codec parameters to decoder context." << std::endl;
        return false;
    }

    if (avcodec_open2(dec_ctx_ptr_.get(), dec, nullptr) < 0)
    {
        std::cerr << "[Capture] ERROR: Failed to open decoder." << std::endl;
        return false;
    }
    std::cout << "[Capture] Started successfully using x11grab." << std::endl;
    return true;
}

void Capture::stop()
{
    stop_flag_ = true;
    raw_frame_queue_->stop(); // 通知队列停止
}

void Capture::run()
{
    auto packet = make_av_packet();
    auto frame = make_av_frame();

    while (!stop_flag_)
    {
        // 使用 get() 获取原始指针
        int ret = av_read_frame(in_fmt_ctx_ptr_.get(), packet.get());
        if (ret < 0)
        {
            if (ret == AVERROR_EOF || avio_feof(in_fmt_ctx_ptr_->pb))
            {
                std::cout << "[Capture] End of stream reached." << std::endl;
            }
            else
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "[Capture] ERROR reading frame: " << errbuf << std::endl;
            }
            break; // 读取失败或结束，退出循环
        }

        if (packet->stream_index == video_stream_index_)
        {
            // 使用 get() 获取原始指针
            ret = avcodec_send_packet(dec_ctx_ptr_.get(), packet.get());
            if (ret == 0)
            {
                // 使用 get() 获取原始指针
                while ((ret = avcodec_receive_frame(dec_ctx_ptr_.get(), frame.get())) == 0)
                {
                    auto frame_to_push = make_av_frame();
                    // 使用 av_frame_move_ref 高效转移帧数据所有权
                    av_frame_move_ref(frame_to_push.get(), frame.get());
                    raw_frame_queue_->push(std::move(frame_to_push));
                }
                if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "[Capture] Error receiving frame from decoder: " << errbuf << std::endl;
                }
            }
            else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "[Capture] Error sending packet to decoder: " << errbuf << std::endl;
            }
        }
        av_packet_unref(packet.get()); // 释放 packet 引用

        // 短暂休眠，避免CPU空转 (可以根据需要调整)
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "[Capture] Thread finished." << std::endl;
}