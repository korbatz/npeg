#ifndef TEST_VLC_COMMON_H
#define TEST_VLC_COMMON_H
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/types.h>
#define VLC_EGENERIC (-1)
#define VLC_SUCCESS 0
#define VLC_ENOMEM (-2)
#define CLOCK_FREQ 1000000LL
#define VLC_TS_0 1
#define VLC_CODEC_RGB24 0x34325652
#define VIDEO_ES 1
#define CAT_INPUT 4
#define SUBCAT_INPUT_DEMUX 403
#define N_(s) (s)
#define msg_Err(obj, ...) ((void)0)
#define msg_Info(obj, ...) ((void)0)
typedef struct vlc_object_t {int unused;} vlc_object_t;
typedef struct demux_sys_t demux_sys_t;
typedef struct stream_t {const uint8_t *data;size_t size;size_t offset;} stream_t;
typedef struct es_out_t {int unused;} es_out_t;
typedef struct es_out_id_t {int unused;} es_out_id_t;
typedef struct {unsigned i_frame_rate, i_frame_rate_base;} video_format_t;
typedef struct {video_format_t video;bool b_packetized;} es_format_t;
typedef struct block_t {uint8_t *p_buffer;int64_t i_pts,i_dts,i_length;} block_t;
float var_InheritFloat(void *, const char *);
void es_format_Init(es_format_t *,int,uint32_t);
void es_format_Clean(es_format_t *);
void video_format_Setup(video_format_t *,uint32_t,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
es_out_id_t *es_out_Add(es_out_t *,es_format_t *);
void es_out_Send(es_out_t *,es_out_id_t *,block_t *);
void es_out_SetPCR(es_out_t *,int64_t);
block_t *block_Alloc(size_t);
void block_Release(block_t *);
ssize_t vlc_stream_Read(stream_t *, void *, size_t);
ssize_t vlc_stream_Peek(stream_t *, const uint8_t **, size_t);
#endif
