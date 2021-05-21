
#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    struct VideoReaderState* state;
    int width, height;
} VideoReader;

bool video_reader_open(VideoReader* reader, const char* filename);
bool video_reader_read_frame(VideoReader* reader, uint8_t* frame_buffer, double* timestamp_s);
bool video_reader_seek_frame(VideoReader* reader, int64_t ts);
void video_reader_close(VideoReader* reader);
int64_t video_reader_seconds_to_stamp(VideoReader* reader, double ts);

double video_duration_in_seconds(VideoReader* reader);

#endif
