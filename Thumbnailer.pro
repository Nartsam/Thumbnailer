QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0




##### To Generate .pri File, You Should Copy Lines Below:


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
    $$PWD/main.cpp \    # Remove in .pri File
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

##### End Copy


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
