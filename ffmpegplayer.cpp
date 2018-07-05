#include "ffmpegplayer.h"
//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

FFmpegPlayer::FFmpegPlayer()
{
    av_register_all();
    avcodec_register_all();
    avformat_network_init();

    pCodecCtx = 0;
    pCodec = 0;
    pCodecParserCtx = 0;
    pFrame = 0;
    pFrameYUV = 0;
    out_buffer = 0;


    screen_w = 0;
    screen_h = 0;
    screen = 0;
    sdlRenderer = 0;
    sdlTexture = 0;
    sdlRect;
    video_tid = 0;

    img_convert_ctx = 0;


    thread_exit=0;
    thread_pause=0;

    pthread_mutex_init(&lock,NULL);
    pthread_cond_init(&cond,NULL);

    cur_ptr = 0;
    cur_size = 0;

    last_width = 0;
    last_height = 0;

    refresh_thread = 0;

    av_init_packet(&packet);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }
}

FFmpegPlayer::~FFmpegPlayer(){
    Close();
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    if(video_tid){
        SDL_WaitThread(video_tid,NULL);
        video_tid = NULL;
    }

    if(refresh_thread != 0){
        pthread_join(refresh_thread,NULL);
        refresh_thread = 0;
    }

    SDL_Quit();
}

void FFmpegPlayer::Close(){
    if(img_convert_ctx){
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
    }

    if(pFrameYUV){
        av_frame_free(&pFrameYUV);
        pFrameYUV = NULL;
    }

    if(pFrame){
        av_frame_free(&pFrame);
        pFrame = NULL;
    }

    if(pCodecCtx){
        avcodec_close(pCodecCtx);
        pCodecCtx = NULL;
    }
}

bool FFmpegPlayer::Open(){
    pFrame=av_frame_alloc();
    pFrameYUV=av_frame_alloc();

    AVCodecID codec_id = AV_CODEC_ID_H264;

    pCodec = avcodec_find_decoder(codec_id);
    if(pCodec==NULL){
        fprintf(stderr,"Codec not found.\n");
        return false;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx){
        printf("Could not allocate video codec context\n");
        return -1;
    }

    pCodecParserCtx=av_parser_init(codec_id);
    if (!pCodecParserCtx){
        printf("Could not allocate video parser context\n");
        return -1;
    }

    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
        fprintf(stderr,"Could not open codec.\n");
        return false;
    }

    pthread_create(&refresh_thread,NULL,RefreshThreadFunc,this);
}

void FFmpegPlayer::ReInitScreen(){
    if(pCodecCtx->width == last_width || pCodecCtx->height == last_height){
        return;
    }

    if(out_buffer){
        av_free(out_buffer);
        out_buffer = NULL;
    }

    if(img_convert_ctx){
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
    }

    if(sdlTexture){
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }

    if(sdlRenderer){
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }

    if(screen){
        SDL_DestroyWindow(screen);
        screen = NULL;
    }

    last_width = pCodecCtx->width;
    last_height = pCodecCtx->height;

    out_buffer=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                                                   pCodecCtx->width,
                                                                   pCodecCtx->height,1));

    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,
                         out_buffer,AV_PIX_FMT_YUV420P,
                         pCodecCtx->width, pCodecCtx->height,1);

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);



    //SDL 2.0 Support for multiple windows
    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_CreateWindow("Simplest ffmpeg player's Window",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              screen_w,
                              screen_h,
                              SDL_WINDOW_OPENGL);

    if(!screen) {
        fprintf(stderr,"SDL: could not create window - exiting:%s\n",SDL_GetError());
        return ;
    }
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    //IYUV: Y + U + V  (3 planes)
    //YV12: Y + V + U  (3 planes)
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);

    sdlRect.x=0;
    sdlRect.y=0;
    sdlRect.w=screen_w;
    sdlRect.h=screen_h;
}

int FFmpegPlayer::ShowImage(){
    bool bComplete = false;
    while (true) {
        pthread_mutex_lock(&lock);
        while (data_list.empty()) {
            pthread_cond_wait(&cond,&lock);
        }
        cur_block = data_list[0];
        data_list.pop_front();
        pthread_mutex_unlock(&lock);

        if(cur_block.size == 0){
            return -1;
        }

        cur_size = cur_block.size;
        cur_ptr = cur_block.data;

        while (cur_size) {
            int len = av_parser_parse2(
                        pCodecParserCtx, pCodecCtx,
                        &packet.data, &packet.size,
                        cur_ptr , cur_size ,
                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

            cur_ptr += len;
            cur_size -= len;

            if(packet.size==0){
                continue;
            }
            else{
                bComplete = true;
                break;
            }
        }

        // a block has been read over
        if(cur_size == 0){
            cur_ptr = NULL;

            delete[] cur_block.data;
            cur_block.data = NULL;
            cur_block.size = 0;
        }

        if(bComplete){
            break;
        }
    }
    int got_picture = 0;
    int ret = avcodec_decode_video2(pCodecCtx,
                                    pFrame,
                                    &got_picture,
                                    &packet);
    av_packet_unref(&packet);

    if(ret <= 0){
        char buf[1024] = {0};
        av_make_error_string(buf,1024,ret);

        fprintf(stderr,"Decode Error.%s\n",buf);
        return -1;
    }

    if(got_picture){
        ReInitScreen();
    }

    if(got_picture){
        sws_scale(img_convert_ctx,
                  (const unsigned char* const*)pFrame->data,
                  pFrame->linesize, 0,
                  pCodecCtx->height,
                  pFrameYUV->data,
                  pFrameYUV->linesize);
        av_frame_unref(pFrame);
        //SDL---------------------------
        SDL_UpdateTexture( sdlTexture,
                           NULL,
                           pFrameYUV->data[0],
                pFrameYUV->linesize[0] );
        SDL_RenderClear( sdlRenderer );
        //SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );
        SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, NULL);
        SDL_RenderPresent( sdlRenderer );
        //SDL End-----------------------
    }

    return 0;
}

void* FFmpegPlayer::RefreshThreadFunc(void* param){
    FFmpegPlayer* player = (FFmpegPlayer*)param;
    player->thread_exit=0;
    player->thread_pause=0;

    while (!player->thread_exit) {
        if(!player->thread_pause){
            SDL_Event event;
            event.type = SFM_REFRESH_EVENT;
            SDL_PushEvent(&event);
        }
        SDL_Delay(40);
    }
    player->thread_exit=0;
    player->thread_pause=0;
    //Break
    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);
    return 0;
}

void FFmpegPlayer::Write(uint8_t* data,uint32_t size){
    DataBlock block;
    block.data = new uint8_t[size + FF_INPUT_BUFFER_PADDING_SIZE];
    block.size = size;
    memcpy(block.data,data,size);

    pthread_mutex_lock(&lock);
    bool empty = data_list.empty();
    data_list.push_back(block);
    if(empty){
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&lock);
}

void FFmpegPlayer::Start(){
    for (;;) {
        //Wait
        SDL_Event event;
        SDL_WaitEvent(&event);
        if(event.type==SFM_REFRESH_EVENT){
            if(ShowImage() < 0){
                return ;
            }
        }
        else if(event.type==SDL_KEYDOWN){
            //Pause
            if(event.key.keysym.sym==SDLK_SPACE){
                thread_pause=!thread_pause;
            }
        }
        else if(event.type==SDL_QUIT){
            thread_exit=1;
        }
        else if(event.type==SFM_BREAK_EVENT){
            break;
        }
    }
    return ;
}
