
#include "VideoReader.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct VideoReaderState
{
    AVRational time_base;
    AVFormatContext* av_format_ctx;
    AVCodecContext*  av_codec_ctx;
    AVFrame*  av_frame;
    AVPacket* av_packet;
    struct SwsContext* sws_scaler_ctx;
    int video_stream_index;
} VideoReaderState;

// av_err2str returns a temporary array. This doesn't work in gcc.
// This function can be used as a replacement for av_err2str.
static const char* av_make_error(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}

bool video_reader_open(VideoReader* reader, const char* filename)
{
    if (!reader)
        return false;

    if (reader->state)
        video_reader_close(reader);

    reader->state = (struct VideoReaderState*) malloc(sizeof(VideoReaderState));
    if (!reader->state)
        return false;

    memset(reader->state, 0, sizeof(VideoReaderState));

    // Open the file using libavformat
    reader->state->av_format_ctx = avformat_alloc_context();
    if (!reader->state->av_format_ctx) {
        printf("Couldn't created AVFormatContext\n");
        video_reader_close(reader);
        return false;
    }

    if (avformat_open_input(&reader->state->av_format_ctx, filename, NULL, NULL) != 0) {
        printf("Couldn't open video file\n");
        video_reader_close(reader);
        return false;
    }

    // some formats such as mpg, do not have headers, so find_stream_info
    // will fill them in.
    avformat_find_stream_info(reader->state->av_format_ctx, NULL);
    
    // Find the first valid video stream inside the file
    reader->state->video_stream_index = -1;
    AVCodecParameters* av_codec_params;
    AVCodec* av_codec;
    for (int i = 0; i < reader->state->av_format_ctx->nb_streams; ++i) {
        av_codec_params = reader->state->av_format_ctx->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);
        if (!av_codec) {
            continue;
        }
        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            reader->state->video_stream_index = i;
            reader->width = av_codec_params->width;
            reader->height = av_codec_params->height;
            reader->state->time_base = reader->state->av_format_ctx->streams[i]->time_base;
            break;
        }
    }
    if (reader->state->video_stream_index == -1) {
        printf("Couldn't find valid video stream inside file\n");
        video_reader_close(reader);
        return false;
    }

    // Set up a codec context for the decoder
    reader->state->av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!reader->state->av_codec_ctx) {
        printf("Couldn't create AVCodecContext\n");
        video_reader_close(reader);
        return false;
    }
    if (avcodec_parameters_to_context(reader->state->av_codec_ctx, av_codec_params) < 0) {
        printf("Couldn't initialize AVCodecContext\n");
        video_reader_close(reader);
        return false;
    }
    if (avcodec_open2(reader->state->av_codec_ctx, av_codec, NULL) < 0) {
        printf("Couldn't open codec\n");
        video_reader_close(reader);
        return false;
    }

    reader->state->av_frame = av_frame_alloc();
    if (!reader->state->av_frame) {
        printf("Couldn't allocate AVFrame\n");
        video_reader_close(reader);
        return false;
    }
    reader->state->av_packet = av_packet_alloc();
    if (!reader->state->av_packet) {
        printf("Couldn't allocate AVPacket\n");
        video_reader_close(reader);
        return false;
    }

    return true;
}

double video_duration_in_seconds(VideoReader* reader)
{
    if (!reader || !reader->state)
        return 0.;

    return (double) reader->state->av_format_ctx->duration / AV_TIME_BASE;
}

int64_t video_reader_seconds_to_stamp(VideoReader* reader, double ts)
{
    double val = ts * reader->state->time_base.den / reader->state->time_base.num;
    return (int64_t) val;
}

bool video_reader_read_frame(VideoReader* reader, uint8_t* frame_buffer, double* pts)
{
    // Decode one frame
    int response;
    while (av_read_frame(reader->state->av_format_ctx, reader->state->av_packet) >= 0)
    {
        if (reader->state->av_packet->stream_index != reader->state->video_stream_index) {
            av_packet_unref(reader->state->av_packet);
            continue;
        }

        response = avcodec_send_packet(reader->state->av_codec_ctx, reader->state->av_packet);
        if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        response = avcodec_receive_frame(reader->state->av_codec_ctx, reader->state->av_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(reader->state->av_packet);
            continue;
        } else if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        av_packet_unref(reader->state->av_packet);
        break;
    }

    *pts = (double)reader->state->av_frame->pts * reader->state->time_base.num / reader->state->time_base.den;
    
    // Set up sws scaler
    if (!reader->state->sws_scaler_ctx) {
        reader->state->sws_scaler_ctx = sws_getContext(reader->width, reader->height, reader->state->av_codec_ctx->pix_fmt,
                                        reader->width, reader->height, AV_PIX_FMT_RGB0,
                                        SWS_BILINEAR, NULL, NULL, NULL);
    }
    if (!reader->state->sws_scaler_ctx) {
        printf("Couldn't initialize sw scaler\n");
        return false;
    }

    uint8_t* dest[4] = { frame_buffer, NULL, NULL, NULL };
    int dest_linesize[4] = { reader->width * 4, 0, 0, 0 };
    sws_scale(reader->state->sws_scaler_ctx, (const uint8_t *const*) reader->state->av_frame->data,
              reader->state->av_frame->linesize, 0, reader->state->av_frame->height, dest, dest_linesize);

    return true;
}

bool video_reader_seek_frame(VideoReader* reader, int64_t ts)
{
    av_seek_frame(reader->state->av_format_ctx, reader->state->video_stream_index, ts, AVSEEK_FLAG_BACKWARD);

    // av_seek_frame takes effect after one frame, so I'm decoding one here
    // so that the next call to video_reader_read_frame() will give the correct
    // frame
    int response;
    while (av_read_frame(reader->state->av_format_ctx, reader->state->av_packet) >= 0) {
        if (reader->state->av_packet->stream_index != reader->state->video_stream_index) {
            av_packet_unref(reader->state->av_packet);
            continue;
        }

        response = avcodec_send_packet(reader->state->av_codec_ctx, reader->state->av_packet);
        if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        response = avcodec_receive_frame(reader->state->av_codec_ctx, reader->state->av_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(reader->state->av_packet);
            continue;
        } else if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        av_packet_unref(reader->state->av_packet);
        break;
    }

    return true;
}

void video_reader_close(VideoReader* reader)
{
    if (!reader || !reader->state)
        return;

    if (reader->state->sws_scaler_ctx)
        sws_freeContext(reader->state->sws_scaler_ctx);
    if (reader->state->av_format_ctx)
    {
        avformat_close_input(&reader->state->av_format_ctx);
        avformat_free_context(reader->state->av_format_ctx);
    }
    if (reader->state->av_frame)
        av_frame_free(&reader->state->av_frame);
    if (reader->state->av_packet)
        av_packet_free(&reader->state->av_packet);
    if (reader->state->av_codec_ctx)
        avcodec_free_context(&reader->state->av_codec_ctx);
    free(reader->state);
    reader->state = NULL;
    reader->width = reader->height = 0;
}
