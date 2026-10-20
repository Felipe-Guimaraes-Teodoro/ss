#pragma once

#include "net.h"
#include <cstdint>

extern "C" {
#include <libavcodec/packet.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct H264Encoder {
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* yuv_frame = nullptr;
    AVPacket* pkt = nullptr;
    int pts = 0;
};

struct EncodedPacket {
    u32 width, height;
    size_t size = 0;
    u8 *data;
};

u8* serialize_packet(EncodedPacket *packet, size_t *out_total_size);
EncodedPacket deserialize_packet(const u8* buffer, u16 *out_sender_id);

H264Encoder* init_h264_encoder(int width, int height, int fps = 60);
EncodedPacket encode_frame(H264Encoder* enc, const uint8_t* raw_bgra_pixels, int width, int height);

struct H264Decoder {
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* yuv_frame = nullptr;
    AVPacket* pkt = nullptr;
    u8* rgba_buffer = nullptr;
    int width = 0;
    int height = 0;
};

H264Decoder* init_h264_decoder(int width, int height);
void free_h264_decoder(H264Decoder *dec); 
u8* decode_frame(H264Decoder* dec, const u8* h264_data, int data_size);

