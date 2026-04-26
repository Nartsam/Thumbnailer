QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

CONFIG += static
CONFIG -= shared

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0



### ===== Generate .dll =====
# TEMPLATE = lib
# DEFINES += DLL_CREATETEST_LIBRARY



#==================== To Generate .pri File, You Should Copy Lines Below ======================
# >>>>>>>>>>>>>>>>>>>>>> Start Copy

# Import FFmpeg
INCLUDEPATH += $$PWD/3rdparty/ffmpeg/include
LIBS += \
	$$PWD/3rdparty/ffmpeg/lib/libavcodec.dll.a \
	$$PWD/3rdparty/ffmpeg/lib/libavfilter.dll.a \
	$$PWD/3rdparty/ffmpeg/lib/libavformat.dll.a \
	$$PWD/3rdparty/ffmpeg/lib/libavutil.dll.a \
	$$PWD/3rdparty/ffmpeg/lib/libswscale.dll.a
# End FFmpeg

# Import GIFWriter
INCLUDEPATH += $$PWD/3rdparty/GIFWriter
HEADERS += \
    $$PWD/3rdparty/GIFWriter/cgif.h \
    $$PWD/3rdparty/GIFWriter/cgif_raw.h \
    $$PWD/3rdparty/GIFWriter/gifencoder.h
SOURCES += \
    $$PWD/3rdparty/GIFWriter/cgif.cpp \
    $$PWD/3rdparty/GIFWriter/cgif_raw.cpp \
    $$PWD/3rdparty/GIFWriter/cgif_rgb.cpp \
    $$PWD/3rdparty/GIFWriter/gifencoder.cpp
# End GIFWriter


INCLUDEPATH += $$PWD/player
INCLUDEPATH += $$PWD/api
INCLUDEPATH += $$PWD/ui

SOURCES += \
    $$PWD/main.cpp \    # Don't Forget Remove This Line in .pri File
    $$PWD/thumbnailer.cpp \
    $$PWD/player/videoplayer.cpp \
    $$PWD/player/ffmpegplayer.cpp \
    $$PWD/player/potplayer.cpp \
    $$PWD/3rdparty/GIFWriter/cgif.cpp \
    $$PWD/ui/thumbnailerdialog.cpp \
    $$PWD/thumblistener.cpp

HEADERS += \
    $$PWD/thumbnailer.h \
    $$PWD/player/videoplayer.h \
    $$PWD/player/ffmpegplayer.h \
    $$PWD/player/potplayer.h \
    $$PWD/ui/thumbnailerdialog.h \
    $$PWD/thumblistener.h \
    $$PWD/api/thumbsgetter.hpp

FORMS += \
    $$PWD/ui/thumbnailerdialog.ui

# >>>>>>>>>>>>>>>>>>>>>> End Copy


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
