/*
 * Minimal, independent NPEG decoder for Noster/Venture's 1998 Amiga format.
 * Based on inspecting original FORM/NPEG files and reconstructing the
 * RLE -> Huffman -> 8x8 inverse DCT decoding sequence.
 *
 * No pixel content is obtained from an external/reference video.
 */
#include "npeg.h"
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX_PIXELS (4096U*4096U)
#define MAX_FRAMES 100000

typedef struct {uint16_t symbol, left, right;} NpegNode;
typedef struct {size_t offset, length;} NpegFrame;
struct NpegMovie {
    uint8_t *file;
    size_t file_size;
    int width, height, frame_count;
    int has_palette;
    uint8_t palette[768];
    NpegNode *tree;
    size_t tree_size;
    NpegFrame *frames;
};

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void fail(char *s, size_t cap, const char *fmt, ...) {
    if (!s || !cap) return;
    va_list args; va_start(args,fmt); vsnprintf(s,cap,fmt,args); va_end(args);
}
void npeg_close(NpegMovie *m) {
    if(!m)return;
    free(m->file); free(m->tree); free(m->frames); free(m);
}
int npeg_width(const NpegMovie *m){return m->width;}
int npeg_height(const NpegMovie *m){return m->height;}
int npeg_frames(const NpegMovie *m){return m->frame_count;}
int npeg_has_palette(const NpegMovie *m){return m->has_palette;}

NpegMovie *npeg_open_memory(const uint8_t *data,size_t size,char *error,size_t cap){
    if(error && cap)error[0]=0;
    if(!data||size<12||size>256U*1024U*1024U){
        fail(error,cap,"Invalid NPEG data size");return NULL;
    }
    NpegMovie *m=calloc(1,sizeof(*m));
    if(!m){fail(error,cap,"Out of memory");return NULL;}
    m->file=malloc(size);
    if(!m->file){fail(error,cap,"Out of memory (input)");npeg_close(m);return NULL;}
    memcpy(m->file,data,size);
    m->file_size=size;
    uint8_t *b=m->file;
    if(memcmp(b,"FORM",4)!=0 || memcmp(b+8,"NPEG",4)!=0){
        fail(error,cap,"Not a FORM/NPEG file");npeg_close(m);return NULL;
    }
    int has_header=0,has_tree=0;
    size_t pos=12;
    while(pos+8<=m->file_size){
        const uint8_t *id=b+pos;
        size_t len=be32(b+pos+4);
        pos+=8;
        if(len>m->file_size-pos){fail(error,cap,"Corrupt chunk length at offset %zu",pos-8);goto bad;}
        const uint8_t *d=b+pos;
        if(!memcmp(id,"NPHD",4)){
            if(len!=14){fail(error,cap,"Unexpected NPHD size %zu",len);goto bad;}
            m->width=be16(d+10);m->height=be16(d+12);
            if(m->width<=0||m->height<=0||m->width%8||m->height%8||
              (uint64_t)m->width*(uint64_t)m->height>MAX_PIXELS){
                fail(error,cap,"Unsupported dimensions %d x %d",m->width,m->height);goto bad;
            }
            has_header=1;
        }else if(!memcmp(id,"NPAL",4)){
            if(len!=768){fail(error,cap,"NPAL must contain 256 RGB triplets");goto bad;}
            memcpy(m->palette,d,768);m->has_palette=1;
        }else if(!memcmp(id,"NTRE",4)){
            if(!len || len%6){fail(error,cap,"Invalid NTRE tree size");goto bad;}
            m->tree_size=len/6;
            if(m->tree_size>65535){fail(error,cap,"Oversized NTRE tree");goto bad;}
            free(m->tree);m->tree=malloc(m->tree_size*sizeof(*m->tree));
            if(!m->tree){fail(error,cap,"Out of memory (tree)");goto bad;}
            for(size_t i=0;i<m->tree_size;i++){
                m->tree[i].symbol=be16(d+6*i);
                m->tree[i].left=be16(d+6*i+2);
                m->tree[i].right=be16(d+6*i+4);
            }
            has_tree=1;
        }else if(!memcmp(id,"NIFR",4)){
            if(m->frame_count==MAX_FRAMES){fail(error,cap,"Too many frames");goto bad;}
            NpegFrame *t=realloc(m->frames,((size_t)m->frame_count+1)*sizeof(*t));
            if(!t){fail(error,cap,"Out of memory (frames)");goto bad;}
            m->frames=t;m->frames[m->frame_count].offset=pos;
            m->frames[m->frame_count].length=len;m->frame_count++;
        }
        pos+=len; /* NPEG intentionally DOES NOT IFF-align odd-length chunks. */
    }
    if(pos!=m->file_size||!has_header||!has_tree||!m->frame_count){
        fail(error,cap,"Missing header/tree/frames, or trailing bytes");goto bad;
    }
    return m;
bad: npeg_close(m);return NULL;
}

/* RLE: first BE32 = Huffman-compressed byte count. High-bit set on a
 * control byte means repeat next byte (control&0x7f)+1 times; otherwise
 * copy that many literal bytes. */
static int unpack_rle(const uint8_t *src, size_t src_len,
                      uint8_t **dest, size_t *dest_len, char *err,size_t cap){
    if(src_len<4){fail(err,cap,"RLE frame too short");return -1;}
    uint32_t n=be32(src);
    if(n>100000000U || n==0){fail(err,cap,"Invalid RLE expanded size: %u",n);return -1;}
    uint8_t *out=malloc((size_t)n+128);
    if(!out){fail(err,cap,"Out of memory (RLE)");return -1;}
    size_t p=4,o=0;
    while(o<n){
        if(p>=src_len){fail(err,cap,"Truncated RLE control");goto bad;}
        int flag=src[p++], count=(flag&127)+1;
        if((size_t)count>n-o+1){fail(err,cap,"RLE expanded too far past declared length");goto bad;}
        if(flag&128){
            if(p>=src_len){fail(err,cap,"Truncated RLE run");goto bad;}
            memset(out+o,src[p++],(size_t)count);
        }else{
            if((size_t)count>src_len-p){fail(err,cap,"Truncated RLE literal run");goto bad;}
            memcpy(out+o,src+p,(size_t)count);p+=(size_t)count;
        }
        o+=(size_t)count;
    }
    /* Some files may carry a padding byte, so don't reject unused tail. */
    *dest=out;*dest_len=n;return 0;
bad:free(out);return -1;
}

/* Huffman tree: each entry = (BE16 symbol, BE16 left, BE16 right).
 * Non-leaf symbol is 0xffff. Root is last tree entry. Read MSB-first,
 * 16-bit big-endian words, emit exactly width*height signed coefficients. */
static int unpack_huffman(const uint8_t *src,size_t src_len,const NpegNode *tree,
       size_t tree_len,uint8_t *out,size_t out_len,char *err,size_t cap){
    size_t p=0,o=0;
    size_t node=tree_len-1;
    while(o<out_len){
        if(node>=tree_len){fail(err,cap,"Invalid Huffman node %zu",node);return -1;}
        if(tree[node].symbol!=65535){
            if(tree[node].symbol>255){fail(err,cap,"Invalid Huffman leaf");return -1;}
            out[o++]=(uint8_t)tree[node].symbol;
            node=tree_len-1;
            continue;
        }
        if(p>=src_len*8){fail(err,cap,"Truncated Huffman bitstream (%zu of %zu bytes)",o,out_len);return -1;}
        /* Original unpacker treats input as a sequence of big-endian words. */
        size_t word=p>>4,bit=15-(p&15);
        if(word*2+1>=src_len){fail(err,cap,"Truncated Huffman word");return -1;}
        uint16_t w=be16(src+word*2);
        node=(w&(1<<bit))?tree[node].right:tree[node].left;
        p++;
    }
    return 0;
}

/* Classic JPEG luminance quantization matrix. Noster's binary stores the
 * values times 10 at code offset 0x304c. */
static const double quant[64]={
  16,11,10,16,24,40,51,61,
  12,12,14,19,26,58,60,55,
  14,13,16,24,40,57,69,56,
  14,17,22,29,51,87,80,62,
  18,22,37,56,68,109,103,77,
  24,35,55,64,81,104,113,92,
  49,64,78,87,103,121,120,101,
  72,92,95,98,112,100,103,99
};

static double basis[8][8];
static void init_basis(void){
    for(int x=0;x<8;x++)for(int u=0;u<8;u++)
        basis[x][u]=(u?0.5:1.0/sqrt(8.0))*cos(M_PI*(2*x+1)*u/16.0);
}
static uint8_t clip_round(double v){
    if(v<=0)return 0;
    if(v>=255)return 255;
    return (uint8_t)floor(v+0.5);
}

static void idct_rgb(const uint8_t *coef, int width,int height,
                    uint8_t *rgb,const uint8_t *palette){
    for(int by=0;by<height;by+=8)for(int bx=0;bx<width;bx+=8){
        double tmp[8][8], transformed[8][8];
        for(int y=0;y<8;y++)for(int x=0;x<8;x++){
            /* First pass over vertical coefficient axis */
            double sum=0;
            for(int v=0;v<8;v++){
                int8_t c=(int8_t)coef[(size_t)(by+v)*width+bx+x];
                sum+=basis[y][v]*(double)c*quant[v*8+x];
            }
            tmp[y][x]=sum;
        }
        for(int y=0;y<8;y++)for(int x=0;x<8;x++){
            double sum=0;
            for(int u=0;u<8;u++)sum+=basis[x][u]*tmp[y][u];
            transformed[y][x]=sum;
        }
        for(int y=0;y<8;y++)for(int x=0;x<8;x++){
            uint8_t value=clip_round(transformed[y][x]);
            size_t pos=((size_t)(by+y)*width+bx+x)*3;
            if(palette){
                rgb[pos]=palette[3*value];rgb[pos+1]=palette[3*value+1];rgb[pos+2]=palette[3*value+2];
            }else rgb[pos]=rgb[pos+1]=rgb[pos+2]=value;
        }
    }
}

int npeg_decode_rgb(NpegMovie *m,int frame,uint8_t *out,int use_palette,
                    char *err,size_t cap){
    if(!m||!out||frame<0||frame>=m->frame_count){fail(err,cap,"Invalid NPEG frame %d",frame);return -1;}
    uint8_t *bitstream=NULL;
    size_t len=0;
    NpegFrame *info=&m->frames[frame];
    if(unpack_rle(m->file+info->offset,info->length,&bitstream,&len,err,cap))return -1;
    size_t pixels=(size_t)m->width*(size_t)m->height;
    uint8_t *coeff=malloc(pixels);
    if(!coeff){fail(err,cap,"Out of memory (coefficients)");free(bitstream);return -1;}
    int ret=unpack_huffman(bitstream,len,m->tree,m->tree_size,coeff,pixels,err,cap);
    free(bitstream);
    if(!ret){
        init_basis();
        idct_rgb(coeff,m->width,m->height,out,use_palette&&m->has_palette?m->palette:NULL);
    }
    free(coeff);
    return ret;
}

NpegMovie *npeg_open(const char *filename,char *error,size_t cap){
    if(error && cap)error[0]=0;
    FILE *f=fopen(filename,"rb");
    if(!f){fail(error,cap,"Cannot open '%s': %s",filename,strerror(errno));return NULL;}
    if(fseek(f,0,SEEK_END)){fclose(f);fail(error,cap,"Seek failed");return NULL;}
    long end=ftell(f);
    if(end<12 || (unsigned long)end>256UL*1024UL*1024UL){
        fclose(f);fail(error,cap,"Invalid file size");return NULL;
    }
    rewind(f);
    uint8_t *buf=malloc((size_t)end);
    if(!buf){fclose(f);fail(error,cap,"Out of memory");return NULL;}
    if(fread(buf,1,(size_t)end,f)!=(size_t)end){
        free(buf);fclose(f);fail(error,cap,"Read failed");return NULL;
    }
    fclose(f);
    NpegMovie *m=npeg_open_memory(buf,(size_t)end,error,cap);
    free(buf);
    return m;
}
