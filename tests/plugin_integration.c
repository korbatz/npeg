/* Host-free VLC-demuxer logic integration tests. NOT a real VLC runtime test. */
#include "vlc_common.h"
#include "vlc_demux.h"
#include <assert.h>
#include <math.h>
#include "../src/vlc_npeg.c"

static int captured_frames;
static uint64_t captured_hash;
static int64_t prev_pts;
static es_out_id_t video_id;

float var_InheritFloat(void *p,const char *key) {(void)p;(void)key;return 7.5f;}
void es_format_Init(es_format_t *f,int cat,uint32_t codec) {(void)cat;(void)codec;memset(f,0,sizeof(*f));}
void es_format_Clean(es_format_t *f) {(void)f;}
void video_format_Setup(video_format_t *f,uint32_t c,unsigned w,unsigned h,unsigned vw,unsigned vh,unsigned sar_n,unsigned sar_d){(void)f;(void)c;(void)w;(void)h;(void)vw;(void)vh;(void)sar_n;(void)sar_d;}
es_out_id_t *es_out_Add(es_out_t *o,es_format_t *f){(void)o;assert(f->video.i_frame_rate==7500);return &video_id;}
void es_out_SetPCR(es_out_t *o,int64_t pts){(void)o;(void)pts;}
block_t *block_Alloc(size_t n){block_t *b=calloc(1,sizeof(*b));if(b)b->p_buffer=malloc(n);return b;}
void block_Release(block_t *b){if(b){free(b->p_buffer);free(b);}}
void es_out_Send(es_out_t *o,es_out_id_t *id,block_t *b){(void)o;assert(id==&video_id);assert(b->i_pts>prev_pts || !captured_frames);prev_pts=b->i_pts;captured_frames++;for(int i=0;i<160*88*3;i++)captured_hash=(captured_hash*131)+b->p_buffer[i];block_Release(b);}
ssize_t vlc_stream_Peek(stream_t *s,const uint8_t **out,size_t n){if(s->offset+n>s->size)return 0;*out=s->data+s->offset;return (ssize_t)n;}
ssize_t vlc_stream_Read(stream_t *s,void *out,size_t n){if(n>s->size-s->offset)n=s->size-s->offset;if(out)memcpy(out,s->data+s->offset,n);s->offset+=n;return (ssize_t)n;}
static int callControl(demux_t *d,int query,...){va_list a;va_start(a,query);int r=Control(d,query,a);va_end(a);return r;}
int main(int argc,char **argv){
    assert(argc==3);
    for(int k=1;k<3;k++){
        FILE *f=fopen(argv[k],"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
        uint8_t *bytes=malloc((size_t)n);assert(bytes);assert(fread(bytes,1,(size_t)n,f)==(size_t)n);fclose(f);
        prev_pts=0;
        stream_t stream={bytes,(size_t)n,0};es_out_t output={0};demux_t demux={.s=&stream,.out=&output};
        assert(Open((vlc_object_t *)&demux)==VLC_SUCCESS);
        int count= k==1 ? 1344 :144;
        int64_t length=0; assert(callControl(&demux,DEMUX_GET_LENGTH,&length)==VLC_SUCCESS);
        assert(llabs(length-(int64_t)(count*1000000.0/7.5))<2);
        assert(demux.pf_demux(&demux)==VLC_DEMUXER_SUCCESS);
        assert(demux.pf_demux(&demux)==VLC_DEMUXER_SUCCESS);
        assert(callControl(&demux,DEMUX_SET_POSITION,0.5,1)==VLC_SUCCESS);
        int64_t t=0;assert(callControl(&demux,DEMUX_GET_TIME,&t)==VLC_SUCCESS);
        assert(t>0 && t<length);
        assert(demux.pf_demux(&demux)==VLC_DEMUXER_SUCCESS);
        assert(callControl(&demux,DEMUX_SET_TIME,(int64_t)(length-1),1)==VLC_SUCCESS);
        assert(demux.pf_demux(&demux)==VLC_DEMUXER_SUCCESS);
        Close((vlc_object_t *)&demux);free(bytes);
        fprintf(stderr,"Mock demux OK: %s (%d frames)\n",argv[k],count);
    }
    assert(captured_frames==8);
    assert(captured_hash!=0);
    puts("PASS: NPEG native demux + RGB frames + timestamp/seek control (mock VLC host)");
}
