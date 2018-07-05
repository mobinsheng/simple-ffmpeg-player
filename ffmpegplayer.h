#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H
#include <stdio.h>
#include <deque>
#include <pthread.h>

#define __STDC_CONSTANT_MACROS

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


struct DataBlock{
    DataBlock(){
        data = NULL;
        size = 0;
    }

    void clear(){
        if(data){
            delete[] data;
            data = NULL;
        }

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
    void Run();
    void Stop();
    void Close(); // close player!
private:
    int ShowImage();
    void ReInitScreen();
private:
    static void* RefreshThreadFunc(void*);
private:
    enum{
        SFM_REFRESH_EVENT = (SDL_USEREVENT + 1),
        SFM_BREAK_EVENT = (SDL_USEREVENT + 2),
    };

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

    pthread_t refresh_thread;

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

    bool running;
};

#endif // FFMPEGPLAYER_H
