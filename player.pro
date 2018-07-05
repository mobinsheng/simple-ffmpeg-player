TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    ffmpegplayer.cpp

LIBS += \
    -lavformat \
    -lavdevice \
    -lavcodec \
    -lavutil \
    -lavfilter \
    -lpostproc \
    -lswresample \
    -lswscale \
    -lpthread \
    -lyuv \
    -lmp4v2 \
    -lopus \
    -lx264 \
    -lopenh264 \
    -lx265 \
    -lfdk-aac \
    -lrtmp \
    -lSDL \
    -lilbc \
    -lvpx \
    -ldl \
    -lasound \
    -lz \
    -lxcb -lxcb-xfixes -lxcb-render -lxcb-shape\
    -lSDL2

HEADERS += \
    ffmpegplayer.h
