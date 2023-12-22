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
    $$PWD/thumbengine.cpp \
    $$PWD/thumbnailer.cpp \
    $$PWD/videoplayer.cpp

HEADERS += \
    $$PWD/VideoInfo.h \
    $$PWD/ffmpegplayer.h \
    $$PWD/thumbengine.h \
    $$PWD/thumbnailer.h \
    $$PWD/videoplayer.h

FORMS += \
    $$PWD/thumbnailer.ui
