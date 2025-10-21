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

// 为 AVFormatContext 自定义 Deleter
struct AVFormatContextDeleter
{
    void operator()(AVFormatContext *ptr) const
    {
        if (!ptr)
            return;
        // 区分输入和输出上下文的关闭方式
        if (ptr->iformat)
        {                               // 输入上下文
            avformat_close_input(&ptr); // 注意这里传递的是指针的地址
        }
        else if (ptr->oformat)
        { // 输出上下文
            if (!(ptr->oformat->flags & AVFMT_NOFILE) && ptr->pb)
            {
                avio_closep(&ptr->pb);
            }
            avformat_free_context(ptr);
        }
        else
        { // 未知或未打开的上下文
            avformat_free_context(ptr);
        }
    }
};
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

// 为 AVCodecContext 自定义 Deleter
struct AVCodecContextDeleter
{
    void operator()(AVCodecContext *ptr) const
    {
        avcodec_free_context(&ptr);
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

// 为 AVFrame 自定义 Deleter
struct AVFrameDeleter
{
    void operator()(AVFrame *ptr) const
    {
        av_frame_free(&ptr);
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

// 为 AVPacket 自定义 Deleter
struct AVPacketDeleter
{
    void operator()(AVPacket *ptr) const
    {
        av_packet_free(&ptr);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

// 为 SwsContext 自定义 Deleter
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