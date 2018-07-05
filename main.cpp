#include <stdio.h>
#include "ffmpegplayer.h"

int main(int argc, char* argv[]){

    FFmpegPlayer player;
    player.Open();
    char filepath[]="test.h264";
    FILE* fp_in = fopen(filepath,"rb");

    while(!feof(fp_in)){
        uint8_t buf[1024] = {0};
        int len = fread(buf,1,1024,fp_in);
        if(len <= 0){
            break;
        }
        player.Write(buf,len);
    }

    player.Write(NULL,0); // end

    player.Run();

    return 0;
}
