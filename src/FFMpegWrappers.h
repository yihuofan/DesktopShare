#pragma once

#include <memory>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

struct AVFormatContextDeleter
{
    void operator()(AVFormatContext *ptr) const
    {
        if (!ptr)
            return;
        // 对于输出上下文，需要检查 pb
        if (ptr->oformat && !(ptr->oformat->flags & AVFMT_NOFILE) && ptr->pb)
        {
            avio_closep(&ptr->pb);
        }
        avformat_free_context(ptr);
    }
};
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

struct AVCodecContextDeleter
{
    void operator()(AVCodecContext *ptr) const
    {
        avcodec_free_context(&ptr);
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
struct AVFrameDeleter
{
    void operator()(AVFrame *ptr) const
    {
        av_frame_free(&ptr);
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

struct AVPacketDeleter
{
    void operator()(AVPacket *ptr) const
    {
        av_packet_free(&ptr);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct SwsContextDeleter
{
    void operator()(SwsContext *ptr) const
    {
        sws_freeContext(ptr);
    }
};
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

// 创建智能指针的工厂函数
inline AVFramePtr make_av_frame()
{
    return AVFramePtr(av_frame_alloc());
}

inline AVPacketPtr make_av_packet()
{
    return AVPacketPtr(av_packet_alloc());
}