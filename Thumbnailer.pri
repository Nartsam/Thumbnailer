# Import FFmpeg
INCLUDEPATH += $$PWD/ffmpeg/include
LIBS += \
	$$PWD/ffmpeg/lib/libavcodec.dll.a \
	$$PWD/ffmpeg/lib/libavfilter.dll.a \
	$$PWD/ffmpeg/lib/libavformat.dll.a \
	$$PWD/ffmpeg/lib/libavutil.dll.a \
	$$PWD/ffmpeg/lib/libswscale.dll.a
# End FFmpeg


SOURCES += \
    $$PWD/ffmpegplayer.cpp \
    $$PWD/thumbnailer.cpp \
    $$PWD/videoplayer.cpp \
    $$PWD/potplayer.cpp \
    $$PWD/GIFWriter/cgif.cpp \
    $$PWD/GIFWriter/cgif_raw.cpp \
    $$PWD/GIFWriter/cgif_rgb.cpp \
    $$PWD/GIFWriter/gifencoder.cpp \
    $$PWD/thumbnailerdialog.cpp

HEADERS += \
    $$PWD/ffmpegplayer.h \
    $$PWD/thumbnailer.h \
    $$PWD/videoplayer.h \
    $$PWD/potplayer.h \
    $$PWD/GIFWriter/cgif.h \
    $$PWD/GIFWriter/cgif_raw.h \
    $$PWD/GIFWriter/gifencoder.h \
    $$PWD/thumbnailerdialog.h

FORMS += \
    $$PWD/thumbnailerdialog.ui
