#include "H264.h"
#include <iostream>
#include <assert.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>

// Sending the whole screen every time would be INSANE so we do this magic
H264Encoder* init_h264_encoder(int width, int height, int fps) {
    H264Encoder* enc = new H264Encoder();

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);

    enc->codec_ctx = avcodec_alloc_context3(codec);
    enc->codec_ctx->width = width;
    enc->codec_ctx->height = height;
    enc->codec_ctx->time_base = {1, fps};
    enc->codec_ctx->framerate = {fps, 1};
    enc->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc->codec_ctx->gop_size = fps/2; // keyframe every x frames

    // enc->codec_ctx->bit_rate = 5000000;

    // enc->codec_ctx->rc_max_rate = 10000000;
    // enc->codec_ctx->rc_min_rate = 1000000;
    // enc->codec_ctx->rc_buffer_size = 15000000;

    av_opt_set(enc->codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(enc->codec_ctx->priv_data, "tune", "zerolatency", 0);

    avcodec_open2(enc->codec_ctx, codec, nullptr);

    enc->yuv_frame = av_frame_alloc();
    enc->yuv_frame->format = enc->codec_ctx->pix_fmt;
    enc->yuv_frame->width = width;
    enc->yuv_frame->height = height;
    av_frame_get_buffer(enc->yuv_frame, 32);

    // converts from screen (RBGA) to what H264 uses (YUC420P)
    enc->sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_BGRA,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    enc->pkt = av_packet_alloc();
    return enc;
}

u8* serialize_packet(EncodedPacket *packet, size_t *out_total_size) {
    size_t header_size = sizeof(packet->width) + sizeof(packet->height) + sizeof(uint64_t) + sizeof(uint16_t);
    *out_total_size = header_size + packet->size;

    uint8_t *buffer = (uint8_t*)malloc(*out_total_size);
    if (!buffer) return NULL;

    size_t offset = 0;

    // leave enough space for this part of the 
    // packet be the server's client port as ID
    offset += sizeof(u16);

    memcpy(buffer + offset, &packet->width, sizeof(packet->width));
    offset += sizeof(packet->width);

    memcpy(buffer + offset, &packet->height, sizeof(packet->height));
    offset += sizeof(packet->height);

    uint64_t net_size = (uint64_t)packet->size;
    memcpy(buffer + offset, &net_size, sizeof(net_size));
    offset += sizeof(net_size);

    if (packet->size > 0 && packet->data != NULL) {
        memcpy(buffer + offset, packet->data, packet->size);
    }

    return buffer;
}

EncodedPacket deserialize_packet(const u8* buffer, uint16_t *out_sender_id) {
    struct EncodedPacket packet;
    size_t offset = 0;

    // read the sender ID injected by the server
    if (out_sender_id != NULL) {
        memcpy(out_sender_id, buffer + offset, sizeof(*out_sender_id));
        offset += sizeof(*out_sender_id);
    }

    memcpy(&packet.width, buffer + offset, sizeof(packet.width));
    offset += sizeof(packet.width);

    memcpy(&packet.height, buffer + offset, sizeof(packet.height));
    offset += sizeof(packet.height);

    uint64_t net_size = 0;
    memcpy(&net_size, buffer + offset, sizeof(net_size));
    offset += sizeof(net_size);
    packet.size = (size_t)net_size;

    packet.data = (uint8_t*)malloc(packet.size);
    if (packet.size > 0 && packet.data != NULL) {
        memcpy(packet.data, buffer + offset, packet.size);
    } else {
        packet.data = NULL;
    }

    return packet;
}

// gets raw pixel data and encodes to H264
EncodedPacket encode_frame(H264Encoder* enc, const uint8_t* raw_bgra_pixels, int width, int height) {
    const uint8_t* src_data[1] = { raw_bgra_pixels };
    int src_linesize[1] = { width * 4 }; // 4 bytes por pixel (BGRA)

    av_frame_make_writable(enc->yuv_frame);
    sws_scale(enc->sws_ctx, src_data, src_linesize, 0, height, 
              enc->yuv_frame->data, enc->yuv_frame->linesize);

    enc->yuv_frame->pts = enc->pts++;

    // Garante que o pacote anterior foi limpo antes de receber um novo
    av_packet_unref(enc->pkt);

    EncodedPacket packet = {};
    packet.width = width;
    packet.height = height;
    packet.size = 0;

    if (avcodec_send_frame(enc->codec_ctx, enc->yuv_frame) >= 0) {
        if (avcodec_receive_packet(enc->codec_ctx, enc->pkt) == 0) {
            packet.size = enc->pkt->size;
            packet.data = enc->pkt->data;
            return packet;
        }
    }

    // Retorna vazio caso o encoder precise de mais frames (raro com zerolatency)
    return packet;
}

H264Decoder* init_h264_decoder(int width, int height) {
    H264Decoder* dec = new H264Decoder();
    dec->width = width;
    dec->height = height;

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "Decoder H.264 not found!\n";
        return nullptr;
    }

    dec->codec_ctx = avcodec_alloc_context3(codec);
    dec->codec_ctx->width = width;
    dec->codec_ctx->height = height;
    dec->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(dec->codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Error while opening decoder H.264!\n";
        return nullptr;
    }

    dec->yuv_frame = av_frame_alloc();
    dec->pkt = av_packet_alloc();

    dec->rgba_buffer = (uint8_t*)malloc(width * height * 4);

    // do opposite of before: YUV420P (Decoder) -> RGBA (Screen)
    dec->sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_YUV420P,
        width, height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    return dec;
}

void free_h264_decoder(H264Decoder *dec) {
    if (!dec) return;
    if (dec->codec_ctx) {
        avcodec_free_context(&dec->codec_ctx);
    }

    if (dec->yuv_frame) {
        av_frame_free(&dec->yuv_frame);
    }

    if (dec->pkt) {
        av_packet_free(&dec->pkt);
    }

    if (dec->sws_ctx) {
        sws_freeContext(dec->sws_ctx);
    }

    if (dec->rgba_buffer) {
        // a memory leak is better than a segfault
        // free(dec->rgba_buffer);
    }

    delete dec;
}

// 2. Get the H.264 bytes from connection and return pointer to RGBA
uint8_t* decode_frame(H264Decoder* dec, const uint8_t* h264_data, int data_size) {
    dec->pkt->data = const_cast<uint8_t*>(h264_data);
    dec->pkt->size = data_size;

    // send compressed packet to decoder queue
    if (avcodec_send_packet(dec->codec_ctx, dec->pkt) >= 0) {
        // extract coded frame
        if (avcodec_receive_frame(dec->codec_ctx, dec->yuv_frame) == 0) {

            // prepare to last struct (RGBA)
            uint8_t* dest_data[1] = { dec->rgba_buffer };
            int dest_linesize[1] = { dec->width * 4 };

            // YUV420P -> RGBA
            sws_scale(
                dec->sws_ctx,
                dec->yuv_frame->data, dec->yuv_frame->linesize, 0, dec->height,
                dest_data, dest_linesize
            );

            return dec->rgba_buffer;
        }
    }

    // returns nullptr in case packet is an incomplete section or needs more frames
    return nullptr;
}

