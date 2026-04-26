#include"gifencoder.h"
#include<QImage>
#include<cstring>

GifEncoder::GifEncoder(QObject *parent):QObject(parent){}

bool GifEncoder::open(QString filename,int width,int height){
    if (pGIF != nullptr) {
        cgif_rgb_close(pGIF);
        pGIF = nullptr;
    }
    CGIFrgb_Config config;
    memset(&config, 0, sizeof(config));
    QByteArray pathBytes = filename.toLocal8Bit();
    config.path = pathBytes.constData();
    config.width = uint16_t(width);
    config.height = uint16_t(height);
    config.genFlags =
        CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
    gifWidth = width;
    gifHeight = height;
    pGIF = cgif_rgb_newgif(&config);
    return pGIF != nullptr;
}

bool GifEncoder::push(QImage &image,int delayTime){
    if (pGIF == nullptr) {
        return false;
    }
    QImage frame = image.scaled(gifWidth, gifHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_RGBA8888);
    CGIFrgb_FrameConfig fconfig;
    memset(&fconfig, 0, sizeof(fconfig));
    fconfig.pImageData = const_cast<uint8_t *>(frame.constBits());
    fconfig.fmtChan = CGIF_CHAN_FMT_RGBA;
    fconfig.genFlags =
        CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
    fconfig.delay = uint16_t(delayTime / 10);
    cgif_rgb_addframe(pGIF, &fconfig);
    return true;
}

bool GifEncoder::close(){
    if (pGIF == nullptr) {
        return false;
    }
    cgif_result r = cgif_rgb_close(pGIF);
    pGIF = nullptr;
    return r == CGIF_OK;
}
