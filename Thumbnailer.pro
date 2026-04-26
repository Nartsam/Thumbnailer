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
# >>>>> Start Copy

# Import FFmpeg
INCLUDEPATH += $$PWD/ffmpeg/include
LIBS += \
	$$PWD/ffmpeg/lib/libavcodec.dll.a \
	$$PWD/ffmpeg/lib/libavfilter.dll.a \
	$$PWD/ffmpeg/lib/libavformat.dll.a \
	$$PWD/ffmpeg/lib/libavutil.dll.a \
	$$PWD/ffmpeg/lib/libswscale.dll.a
# End FFmpeg



INCLUDEPATH += $$PWD/player

SOURCES += \
    $$PWD/main.cpp \    # Don't Forget Remove This Line in .pri File
    $$PWD/thumbnailer.cpp \
    $$PWD/player/videoplayer.cpp \
    $$PWD/player/ffmpegplayer.cpp \
    $$PWD/player/potplayer.cpp \
    $$PWD/GIFWriter/cgif.cpp \
    $$PWD/GIFWriter/cgif_raw.cpp \
    $$PWD/GIFWriter/cgif_rgb.cpp \
    $$PWD/GIFWriter/gifencoder.cpp \
    $$PWD/thumbnailerdialog.cpp \
    $$PWD/thumblistener.cpp

HEADERS += \
    $$PWD/thumbnailer.h \
    $$PWD/player/videoplayer.h \
    $$PWD/player/ffmpegplayer.h \
    $$PWD/player/potplayer.h \
    $$PWD/GIFWriter/cgif.h \
    $$PWD/GIFWriter/cgif_raw.h \
    $$PWD/GIFWriter/gifencoder.h \
    $$PWD/thumbnailerdialog.h \
    $$PWD/thumblistener.h \
    $$PWD/thumbsgetter.hpp

FORMS += \
    $$PWD/thumbnailerdialog.ui

# >>>>> End Copy


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
