#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H
#include <stdio.h>
#include <deque>
#include <pthread.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
};
#endif
#endif

struct DataBlock{
    DataBlock(){
        data = NULL;
        size = 0;
    }

    uint8_t* data;
    uint32_t size;
};

class FFmpegPlayer
{
public:
    FFmpegPlayer();
    ~FFmpegPlayer();
    bool  Open(); // open player! not open a media file!
    void Write(uint8_t* data,uint32_t size);
    void Start();
    void Close(); // close player!
private:
    int ShowImage();
    void ReInitScreen();
private:
    static void* MainThreadFunc(void*);
    static int RefreshThreadFunc(void*);  // sdl
    static void* RefreshThreadFuncWrap(void*);
    static int ReadBuffer(void *opaque, uint8_t *buf, int buf_size);
private:
    AVCodecContext	*pCodecCtx;
    AVCodec			*pCodec;
    AVCodecParserContext *pCodecParserCtx;
    AVFrame	*pFrame,*pFrameYUV;
    unsigned char *out_buffer;


    //------------SDL----------------
    int screen_w,screen_h;
    SDL_Window *screen;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread *video_tid;

    AVPacket packet;

    struct SwsContext *img_convert_ctx;

    pthread_t main_thread;

    int thread_exit;
    int thread_pause;

    std::deque<DataBlock> data_list;
    pthread_mutex_t lock;
    pthread_cond_t cond;

    DataBlock cur_block;
    uint8_t* cur_ptr;
    int cur_size;

    int last_width;
    int last_height;
};

#endif // FFMPEGPLAYER_H
