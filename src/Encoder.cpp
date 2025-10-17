#include "Encoder.h"
#include <iostream>

Encoder::Encoder(std::shared_ptr<ThreadSafeQueue<AVFramePtr>> raw_q,
                 std::shared_ptr<ThreadSafeQueue<AVPacketPtr>> encoded_q)
    : raw_frame_queue_(raw_q), encoded_packet_queue_(encoded_q) {}

Encoder::~Encoder() { stop(); }

bool Encoder::start(int width, int height)
{
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    enc_ctx_.reset(avcodec_alloc_context3(codec));

    enc_ctx_->width = width;
    enc_ctx_->height = height;
    enc_ctx_->bit_rate = 4000000;
    enc_ctx_->time_base = {1, 60};
    enc_ctx_->framerate = {60, 1};
    enc_ctx_->gop_size = 30;
    enc_ctx_->max_b_frames = 1;
    enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    av_opt_set(enc_ctx_->priv_data, "preset", "ultrafast", 0);
    av_opt_set(enc_ctx_->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(enc_ctx_.get(), codec, nullptr) < 0)
    {
        std::cerr << "[Encoder] ERROR: Could not open codec." << std::endl;
        return false;
    }

    scaled_frame_ = make_av_frame();
    scaled_frame_->width = width;
    scaled_frame_->height = height;
    scaled_frame_->format = enc_ctx_->pix_fmt;
    av_frame_get_buffer(scaled_frame_.get(), 0);

    std::cout << "[Encoder] Started successfully." << std::endl;
    return true;
}

void Encoder::stop()
{
    stop_flag_ = true;
    raw_frame_queue_->stop();
    encoded_packet_queue_->stop();
}

void Encoder::run()
{
    AVFramePtr raw_frame;
    auto packet = make_av_packet();

    while (!stop_flag_ && raw_frame_queue_->wait_and_pop(raw_frame))
    {
        if (!raw_frame)
            continue;

        if (!sws_ctx_)
        {
            sws_ctx_.reset(sws_getContext(raw_frame->width, raw_frame->height, (AVPixelFormat)raw_frame->format,
                                          enc_ctx_->width, enc_ctx_->height, enc_ctx_->pix_fmt,
                                          SWS_BILINEAR, nullptr, nullptr, nullptr));
        }

        sws_scale(sws_ctx_.get(), (const uint8_t *const *)raw_frame->data, raw_frame->linesize, 0, raw_frame->height,
                  scaled_frame_->data, scaled_frame_->linesize);

        scaled_frame_->pts = frame_count_++;

        if (avcodec_send_frame(enc_ctx_.get(), scaled_frame_.get()) == 0)
        {
            while (avcodec_receive_packet(enc_ctx_.get(), packet.get()) == 0)
            {
                auto packet_to_push = make_av_packet();
                av_packet_move_ref(packet_to_push.get(), packet.get());
                encoded_packet_queue_->push(std::move(packet_to_push));
            }
        }
    }

    avcodec_send_frame(enc_ctx_.get(), nullptr);
    while (avcodec_receive_packet(enc_ctx_.get(), packet.get()) == 0)
    {
        auto packet_to_push = make_av_packet();
        av_packet_move_ref(packet_to_push.get(), packet.get());
        encoded_packet_queue_->push(std::move(packet_to_push));
    }

    std::cout << "[Encoder] Thread finished." << std::endl;
}