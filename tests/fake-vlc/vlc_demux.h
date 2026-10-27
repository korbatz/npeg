#ifndef TEST_VLC_DEMUX_H
#define TEST_VLC_DEMUX_H
#include "vlc_common.h"
#define VLC_DEMUXER_EOF 0
#define VLC_DEMUXER_SUCCESS 1
#define VLC_DEMUXER_EGENERIC (-1)
enum {DEMUX_CAN_SEEK=0, DEMUX_CAN_PAUSE=2, DEMUX_CAN_CONTROL_PACE=3,
  DEMUX_GET_LENGTH=0x302,DEMUX_GET_TIME=0x303,DEMUX_GET_POSITION=0x300,
  DEMUX_SET_TIME=0x305,DEMUX_SET_POSITION=0x301,DEMUX_GET_FPS=0x310};
typedef struct demux_t {
    stream_t *s; es_out_t *out; demux_sys_t *p_sys;
    int (*pf_demux)(struct demux_t *);
    int (*pf_control)(struct demux_t *, int, va_list);
} demux_t;
#endif
