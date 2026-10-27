#define _POSIX_C_SOURCE 200809L
#include "npeg.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void help(void){
    puts("NPEG native decoder / MP4 exporter (Amiga Venture/Noster 1998)\n"
         "Usage: ./npeg-convert FILE.npeg [-o film.mp4] [--fps 7.5] [--scale 4]\n"
         "       [--audio music.mp2] [--gray] [--frame 0 --ppm out.ppm]\n"
         "       ./npeg-convert FILE.npeg --info\n"
         "\n"
         "All video pixels are decoded FROM NPEG, never from a reference MP4.\n"
         "Audio is optional and not embedded in NPEG. Default FPS 7.5 is inferred\n"
         "from preserved reference, not proven by the NPHD header.\n");
}
static int decode_ppm(NpegMovie *m,int i,const char *filename,int palette){
    char err[256];size_t len=(size_t)npeg_width(m)*npeg_height(m)*3;
    unsigned char *rgb=malloc(len);
    if(!rgb){fputs("Out of memory\n",stderr);return 1;}
    if(npeg_decode_rgb(m,i,rgb,palette,err,sizeof(err))){fprintf(stderr,"Decode: %s\n",err);free(rgb);return 1;}
    FILE *f=fopen(filename,"wb");
    if(!f){perror(filename);free(rgb);return 1;}
    fprintf(f,"P6\n%d %d\n255\n",npeg_width(m),npeg_height(m));
    int bad=fwrite(rgb,1,len,f)!=len || fclose(f)!=0;
    free(rgb);
    if(bad){fputs("Write failed\n",stderr);return 1;}
    printf("Frame %d exported to %s\n",i,filename);
    return 0;
}
static int convert(NpegMovie *m,const char *filename,const char *audio,
                   const char *fps,int scale,int palette){
    int pipes[2];if(pipe(pipes)){perror("pipe");return 1;}
    char size[64],outsize[64];
    snprintf(size,sizeof(size),"%dx%d",npeg_width(m),npeg_height(m));
    snprintf(outsize,sizeof(outsize),"scale=%d:%d:flags=neighbor,format=yuv420p",
      npeg_width(m)*scale,npeg_height(m)*scale);
    pid_t child=fork();
    if(child<0){perror("fork");close(pipes[0]);close(pipes[1]);return 1;}
    if(child==0){
        dup2(pipes[0],STDIN_FILENO);close(pipes[0]);close(pipes[1]);
        /* Pass arguments directly, never through the shell. */
        char *args[40];int k=0;
        args[k++]="ffmpeg";args[k++]="-hide_banner";args[k++]="-loglevel";args[k++]="error";
        args[k++]="-y";args[k++]="-f";args[k++]="rawvideo";
        args[k++]="-pix_fmt";args[k++]="rgb24";
        args[k++]="-video_size";args[k++]=size;
        args[k++]="-framerate";args[k++]=(char*)fps;
        args[k++]="-i";args[k++]="pipe:0";
        if(audio){args[k++]="-i";args[k++]=(char*)audio;}
        args[k++]="-vf";args[k++]=outsize;
        args[k++]="-c:v";args[k++]="libx264";
        args[k++]="-preset";args[k++]="medium";
        args[k++]="-crf";args[k++]="18";
        args[k++]="-pix_fmt";args[k++]="yuv420p";
        if(audio){args[k++]="-c:a";args[k++]="aac";args[k++]="-b:a";args[k++]="160k";args[k++]="-shortest";}
        args[k++]="-movflags";args[k++]="+faststart";
        args[k++]=(char*)filename;args[k]=NULL;
        execvp(args[0],args);
        perror("ffmpeg not found");_exit(127);
    }
    close(pipes[0]);
    FILE *stream=fdopen(pipes[1],"wb");
    size_t len=(size_t)npeg_width(m)*npeg_height(m)*3;
    unsigned char *rgb=malloc(len);
    int bad=0;
    if(!stream || !rgb){fputs("Out of memory / pipe failure\n",stderr);bad=1;}
    for(int i=0;!bad && i<npeg_frames(m);i++){
        char err[256];
        if(npeg_decode_rgb(m,i,rgb,palette,err,sizeof(err))){
            fprintf(stderr,"Frame %d of %d: %s\n",i+1,npeg_frames(m),err);bad=1;break;
        }
        if(fwrite(rgb,1,len,stream)!=len){perror("ffmpeg pipe");bad=1;break;}
        if(i%100==0||i==npeg_frames(m)-1)
            fprintf(stderr,"\rDecoded %d / %d NPEG frames",i+1,npeg_frames(m));
    }
    fputs("\n",stderr);
    free(rgb);
    if(stream){if(fclose(stream))bad=1;}else close(pipes[1]);
    int status=0;
    if(waitpid(child,&status,0)<0 || !WIFEXITED(status)||WEXITSTATUS(status)!=0){
        fprintf(stderr,"ffmpeg failed; verify ffmpeg and H.264 encoder are installed\n");bad=1;
    }
    if(bad){remove(filename);return 1;}
    printf("Saved %s (%d genuine decoded NPEG frames)\n",filename,npeg_frames(m));
    return 0;
}
int main(int argc,char **argv){
    const char *input=NULL,*output=NULL,*audio=NULL,*fps="7.5",*ppm=NULL;
    int palette=1,scale=4,frame=-1,info=0;
    for(int i=1;i<argc;i++){
        const char *v=argv[i];
        if(!strcmp(v,"--help")||!strcmp(v,"-h")){help();return 0;}
        else if(!strcmp(v,"-o")&&i+1<argc)output=argv[++i];
        else if(!strcmp(v,"--fps")&&i+1<argc)fps=argv[++i];
        else if(!strcmp(v,"--scale")&&i+1<argc)scale=atoi(argv[++i]);
        else if(!strcmp(v,"--audio")&&i+1<argc)audio=argv[++i];
        else if(!strcmp(v,"--ppm")&&i+1<argc)ppm=argv[++i];
        else if(!strcmp(v,"--frame")&&i+1<argc)frame=atoi(argv[++i]);
        else if(!strcmp(v,"--gray"))palette=0;
        else if(!strcmp(v,"--info"))info=1;
        else if(v[0]=='-'){fprintf(stderr,"Unknown option: %s\n",v);return 2;}
        else if(!input)input=v;
        else {fputs("Only one input NPEG allowed\n",stderr);return 2;}
    }
    if(!input||scale<1||scale>16||strtod(fps,NULL)<=0){help();return 2;}
    char err[256];NpegMovie *m=npeg_open(input,err,sizeof(err));
    if(!m){fprintf(stderr,"Error: %s\n",err);return 1;}
    printf("NPEG: %s, %d x %d, %d frames, palette: %s\n",input,
      npeg_width(m),npeg_height(m),npeg_frames(m),npeg_has_palette(m)?"yes":"no");
    int ret=0;
    if(ppm){ret=decode_ppm(m,frame<0?0:frame,ppm,palette);}
    else if(!info){
        if(!output){fputs("Use -o result.mp4, --ppm frame.ppm, or --info\n",stderr);ret=2;}
        else ret=convert(m,output,audio,fps,scale,palette);
    }
    npeg_close(m);return ret;
}
