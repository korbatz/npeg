/*
 * VLC 3.x NPEG demuxer (Noster / Venture Amiga video, 1998)
 *
 * Implements native NPEG playback by decoding each compressed frame with
 * libnpeg and delivering RGB24 frames to VLC's built-in raw-video decoder.
 * Requires VLC 3.x internal plugin headers (vlc-devel / libvlccore-dev).
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "npeg.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define NPEG_MAX_FILE (256U * 1024U * 1024U)
#define NPEG_FPS_SCALE 1000U

struct demux_sys_t
{
    NpegMovie *movie;
    es_out_id_t *video;
    int frame;
    int frame_count;
    unsigned fps_num;
    int64_t duration;
    size_t rgb_bytes;
};

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int Demux(demux_t *);
static int Control(demux_t *, int, va_list);

vlc_module_begin()
    set_shortname("NPEG")
    set_description("Noster NPEG Amiga video demuxer")
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_capability("demux", 100)
    set_callbacks(Open, Close)
    add_shortcut("npeg")
    add_float("npeg-fps", 7.5, "NPEG playback FPS",
              "Original frame rate is not stored in available NPEG samples; 7.5 FPS is inferred from a preserved film.", false)
        change_float_range(1.0, 120.0)
vlc_module_end()

static int64_t frame_time(const demux_sys_t *s, int frame)
{
    /* Integer calculation prevents time drift across long films. */
    return ((int64_t)frame * (int64_t)CLOCK_FREQ * NPEG_FPS_SCALE
            + s->fps_num / 2) / s->fps_num;
}

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    const uint8_t *head = NULL;
    if (vlc_stream_Peek(demux->s, &head, 12) < 12 ||
        memcmp(head, "FORM", 4) || memcmp(head + 8, "NPEG", 4))
        return VLC_EGENERIC;

    /* Read from VLC's access layer: works for local files and stream URLs. */
    uint8_t *buf = NULL;
    size_t cap = 0, length = 0;
    for (;;) {
        if (length == cap) {
            if (cap == NPEG_MAX_FILE) {
                msg_Err(demux, "NPEG file exceeds 256 MiB safety limit");
                free(buf);
                return VLC_EGENERIC;
            }
            size_t newcap = cap ? cap * 2 : 64 * 1024;
            if (newcap > NPEG_MAX_FILE) newcap = NPEG_MAX_FILE;
            uint8_t *p = realloc(buf, newcap);
            if (!p) { free(buf); return VLC_ENOMEM; }
            buf = p;
            cap = newcap;
        }
        ssize_t n = vlc_stream_Read(demux->s, buf + length, cap - length);
        if (n < 0) { free(buf); return VLC_EGENERIC; }
        if (n == 0) break;
        length += (size_t)n;
    }

    char error[256];
    NpegMovie *movie = npeg_open_memory(buf, length, error, sizeof(error));
    free(buf);
    if (!movie) {
        msg_Err(demux, "NPEG parser: %s", error);
        return VLC_EGENERIC;
    }

    float fps = var_InheritFloat(demux, "npeg-fps");
    if (!(fps >= 1.0f && fps <= 120.0f)) fps = 7.5f;

    demux_sys_t *s = calloc(1, sizeof(*s));
    if (!s) { npeg_close(movie); return VLC_ENOMEM; }
    s->movie = movie;
    s->frame_count = npeg_frames(movie);
    s->fps_num = (unsigned)lroundf(fps * NPEG_FPS_SCALE);
    s->rgb_bytes = (size_t)npeg_width(movie) * npeg_height(movie) * 3;
    s->duration = frame_time(s, s->frame_count);

    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_RGB24);
    video_format_Setup(&fmt.video, VLC_CODEC_RGB24,
                       npeg_width(movie), npeg_height(movie),
                       npeg_width(movie), npeg_height(movie), 1, 1);
    fmt.video.i_frame_rate = s->fps_num;
    fmt.video.i_frame_rate_base = NPEG_FPS_SCALE;
    fmt.b_packetized = true;
    s->video = es_out_Add(demux->out, &fmt);
    es_format_Clean(&fmt);
    if (!s->video) {
        npeg_close(movie);
        free(s);
        return VLC_EGENERIC;
    }

    demux->p_sys = s;
    demux->pf_demux = Demux;
    demux->pf_control = Control;
    msg_Info(demux, "NPEG: %dx%d, %d frames, %.3f fps, %s",
             npeg_width(movie), npeg_height(movie), s->frame_count,
             (double)s->fps_num / NPEG_FPS_SCALE,
             npeg_has_palette(movie) ? "palette" : "grayscale");
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *s = demux->p_sys;
    if (s) {
        npeg_close(s->movie);
        free(s);
    }
}

static int Demux(demux_t *demux)
{
    demux_sys_t *s = demux->p_sys;
    if (s->frame >= s->frame_count) return VLC_DEMUXER_EOF;

    block_t *block = block_Alloc(s->rgb_bytes);
    if (!block) return VLC_DEMUXER_EGENERIC;

    char error[256];
    if (npeg_decode_rgb(s->movie, s->frame, block->p_buffer, 1,
                        error, sizeof(error)) != 0) {
        msg_Err(demux, "Frame %d: %s", s->frame, error);
        block_Release(block);
        return VLC_DEMUXER_EGENERIC;
    }

    int64_t pts = VLC_TS_0 + frame_time(s, s->frame);
    block->i_pts = block->i_dts = pts;
    block->i_length = frame_time(s, s->frame + 1) - frame_time(s, s->frame);
    es_out_SetPCR(demux->out, pts);
    es_out_Send(demux->out, s->video, block);
    s->frame++;
    return VLC_DEMUXER_SUCCESS;
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *s = demux->p_sys;
    switch (query) {
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            return VLC_SUCCESS;
        case DEMUX_GET_LENGTH:
            *va_arg(args, int64_t *) = s->duration;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            *va_arg(args, int64_t *) = frame_time(s, s->frame);
            return VLC_SUCCESS;
        case DEMUX_GET_POSITION:
            *va_arg(args, double *) =
                (double)s->frame / s->frame_count;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME: {
            int64_t t = va_arg(args, int64_t);
            (void)va_arg(args, int); /* precise (promoted bool) */
            if (t < 0) t = 0;
            if (t > s->duration) t = s->duration;
            int64_t index = t * (int64_t)s->fps_num /
                            ((int64_t)CLOCK_FREQ * NPEG_FPS_SCALE);
            s->frame = (int)index;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION: {
            double position = va_arg(args, double);
            (void)va_arg(args, int); /* precise (promoted bool) */
            if (!(position >= 0.0)) position = 0.0;
            if (position > 1.0) position = 1.0;
            s->frame = (int)(position * s->frame_count);
            return VLC_SUCCESS;
        }
        case DEMUX_GET_FPS:
            *va_arg(args, double *) =
                (double)s->fps_num / NPEG_FPS_SCALE;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}
