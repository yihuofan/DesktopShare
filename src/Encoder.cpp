#include "Encoder.h"
#include <iostream>

Encoder::Encoder(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> raw_q,
                 std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_q)
    : raw_frame_queue_(raw_q), encoded_packet_queue_(encoded_q) {}

Encoder::~Encoder()
{
    stop();
    // 智能指针会自动释放资源
}

bool Encoder::start(int width, int height)
{
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
    {
        std::cerr << "[Encoder] ERROR: Codec 'libx264' not found." << std::endl;
        return false;
    }

    AVCodecContext *ctx_raw = avcodec_alloc_context3(codec);
    if (!ctx_raw)
    {
        std::cerr << "[Encoder] ERROR: Could not allocate encoder context." << std::endl;
        return false;
    }
    enc_ctx_.reset(ctx_raw);

    // 设置编码参数
    enc_ctx_->width = width;
    enc_ctx_->height = height;
    enc_ctx_->bit_rate = 4000000;           // 4 Mbps
    enc_ctx_->time_base = {1, 30};          // 时间基，假设帧率为 30fps
    enc_ctx_->framerate = {30, 1};          // 帧率
    enc_ctx_->gop_size = 30;                // 每 30 帧一个I帧
    enc_ctx_->max_b_frames = 1;             // 允许的最大B帧数量
    enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P; // libx264 通常需要 YUV420P

    // 设置编码速度和延迟优化选项
    av_opt_set(enc_ctx_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(enc_ctx_->priv_data, "tune", "zerolatency", 0);

    // 打开编码器
    if (avcodec_open2(enc_ctx_.get(), codec, nullptr) < 0)
    {
        std::cerr << "[Encoder] ERROR: Could not open codec 'libx264'." << std::endl;
        return false;
    }

    // 准备用于格式转换的目标帧
    scaled_frame_ = make_av_frame();
    if (!scaled_frame_)
    {
        std::cerr << "[Encoder] ERROR: Could not allocate scaled frame." << std::endl;
        return false;
    }
    scaled_frame_->width = width;
    scaled_frame_->height = height;
    scaled_frame_->format = enc_ctx_->pix_fmt;
    // 为 scaled_frame 分配 buffer
    if (av_frame_get_buffer(scaled_frame_.get(), 0) < 0)
    {
        std::cerr << "[Encoder] ERROR: Could not allocate buffer for scaled frame." << std::endl;
        return false;
    }

    std::cout << "[Encoder] Started successfully with libx264." << std::endl;
    return true;
}

void Encoder::stop()
{
    stop_flag_ = true;
    raw_frame_queue_->stop();      // 通知上游队列停止
    encoded_packet_queue_->stop(); // 通知下游队列停止
}

void Encoder::run()
{
    AVFramePtr raw_frame;
    auto packet = make_av_packet();
    frame_count_ = 0; // 重置帧计数器

    while (!stop_flag_)
    {
        // 从原始帧队列中等待并获取数据
        if (!raw_frame_queue_->wait_and_pop(raw_frame))
        {
            // 如果返回 false，检查是否是停止信号
            if (stop_flag_)
            {
                break; // 收到停止信号且队列已空，退出循环
            }
            continue; // 队列暂时为空，继续等待
        }

        if (!raw_frame)
            continue; // 以防万一弹出的是空指针

        // 惰性初始化 SwsContext
        if (!sws_ctx_)
        {
            sws_ctx_.reset(sws_getContext(raw_frame->width, raw_frame->height, (AVPixelFormat)raw_frame->format,
                                          enc_ctx_->width, enc_ctx_->height, enc_ctx_->pix_fmt,
                                          SWS_BILINEAR, nullptr, nullptr, nullptr));
            if (!sws_ctx_)
            {
                std::cerr << "[Encoder] ERROR: Failed to create SwsContext." << std::endl;
                break; // 无法转换，退出
            }
        }

        // 执行像素格式转换
        sws_scale(sws_ctx_.get(), (const uint8_t *const *)raw_frame->data, raw_frame->linesize, 0, raw_frame->height,
                  scaled_frame_->data, scaled_frame_->linesize);

        // 设置 scaled_frame 的 PTS (Presentation Timestamp)
        // 使用简单的帧计数作为 PTS，基于编码器的时间基
        scaled_frame_->pts = frame_count_++;

        // 将转换后的帧发送给编码器
        int ret = avcodec_send_frame(enc_ctx_.get(), scaled_frame_.get());
        if (ret == 0)
        {
            // 循环接收编码后的数据包
            while (ret == 0)
            {
                ret = avcodec_receive_packet(enc_ctx_.get(), packet.get());
                if (ret == 0)
                {
                    // 成功收到 packet
                    auto packet_to_push = make_av_packet();
                    // 使用 move_ref 高效转移 packet 数据
                    av_packet_move_ref(packet_to_push.get(), packet.get());
                    encoded_packet_queue_->push(std::move(packet_to_push));
                }
                else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "[Encoder] Error receiving packet from encoder: " << errbuf << std::endl;
                }
            }
        }
        else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[Encoder] Error sending frame to encoder: " << errbuf << std::endl;
        }
    }

    // --- 刷新编码器 ---
    // 发送 nullptr 帧以通知编码器结束
    avcodec_send_frame(enc_ctx_.get(), nullptr);
    while (true)
    {
        int ret = avcodec_receive_packet(enc_ctx_.get(), packet.get());
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
        {
            break; // 编码器已完全刷新或需要更多输入（但我们已结束）
        }
        else if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "[Encoder] Error flushing encoder: " << errbuf << std::endl;
            break;
        }
        // 处理最后剩余的 packet
        auto packet_to_push = make_av_packet();
        av_packet_move_ref(packet_to_push.get(), packet.get());
        encoded_packet_queue_->push(std::move(packet_to_push));
    }

    std::cout << "[Encoder] Thread finished." << std::endl;
}