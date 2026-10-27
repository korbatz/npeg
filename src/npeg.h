#ifndef NPEG_H
#define NPEG_H
#include <stddef.h>
#include <stdint.h>

typedef struct NpegMovie NpegMovie;

/* Frame pixels are RGB24, tightly packed, with no stride padding. */
NpegMovie *npeg_open(const char *filename, char *error, size_t error_capacity);
/* Copies a complete FORM/NPEG file from caller-owned memory. */
NpegMovie *npeg_open_memory(const uint8_t *data, size_t size,
                            char *error, size_t error_capacity);
void npeg_close(NpegMovie *m);
int npeg_width(const NpegMovie *m);
int npeg_height(const NpegMovie *m);
int npeg_frames(const NpegMovie *m);
int npeg_has_palette(const NpegMovie *m);
/* Zero on success; nonzero on error. Output: width*height*3 bytes. */
int npeg_decode_rgb(NpegMovie *m, int frame_number, uint8_t *output, int use_palette,
                    char *error, size_t error_capacity);
#endif
