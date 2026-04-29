#include"gifencoder.h"
#include<QImage>
#include<cstring>

GifEncoder::GifEncoder(QObject *parent):QObject(parent){}

void GifEncoder::setQuality(int quality){
    m_quality = qBound(1, quality, 10);
}

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
    m_prevFrame = QImage();
    m_pendingDelay = 0;
    pGIF = cgif_rgb_newgif(&config);
    return pGIF != nullptr;
}

bool GifEncoder::push(QImage &image,int delayTime){
    if (pGIF == nullptr) {
        return false;
    }
    QImage frame = image.scaled(gifWidth, gifHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_RGBA8888);

    /*
        quality → 压缩参数映射:
        10~9: 255色, Floyd-Steinberg抖动, 不跳帧 (与原始行为一致)
         8~7: 127色, Floyd-Steinberg抖动, 不跳帧
         6~5:  63色, 无抖动, 差异<0.5%跳帧
         4~3:  31色, 无抖动, 差异<1.0%跳帧
         2~1:  15色, 无抖动, 差异<2.0%跳帧
    */
    uint8_t depthMax;
    uint8_t dithering;          // CGIFrgb_FrameConfig convention: 0=default(FS), 1=none, 2=FS, 3=Sierra
    double frameSkipThreshold;

    if (m_quality >= 9) {
        depthMax = 8; dithering = 2; frameSkipThreshold = 0;
    } else if (m_quality >= 7) {
        depthMax = 7; dithering = 2; frameSkipThreshold = 0;
    } else if (m_quality >= 5) {
        depthMax = 6; dithering = 1; frameSkipThreshold = 0.005;
    } else if (m_quality >= 3) {
        depthMax = 5; dithering = 1; frameSkipThreshold = 0.01;
    } else {
        depthMax = 4; dithering = 1; frameSkipThreshold = 0.02;
    }

    // 帧跳过: 与上一帧逐像素比较, 差异低于阈值则累计延时并跳过本帧
    if (frameSkipThreshold > 0 && !m_prevFrame.isNull()) {
        int diffCount = 0;
        int totalPixels = gifWidth * gifHeight;
        const uint8_t *cur = frame.constBits();
        const uint8_t *prev = m_prevFrame.constBits();
        for (int i = 0; i < totalPixels; ++i) {
            int off = i * 4;
            if (cur[off] != prev[off] || cur[off+1] != prev[off+1] || cur[off+2] != prev[off+2]) {
                if (++diffCount >= totalPixels * frameSkipThreshold) break; // 提前退出
            }
        }
        if (static_cast<double>(diffCount) / totalPixels < frameSkipThreshold) {
            m_pendingDelay += delayTime;
            return true;
        }
    }

    if (frameSkipThreshold > 0) {
        m_prevFrame = frame.copy();
    }

    CGIFrgb_FrameConfig fconfig;
    memset(&fconfig, 0, sizeof(fconfig));
    fconfig.pImageData = const_cast<uint8_t *>(frame.constBits());
    fconfig.fmtChan = CGIF_CHAN_FMT_RGBA;
    fconfig.genFlags =
        CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
    fconfig.delay = uint16_t((delayTime + m_pendingDelay) / 10);
    fconfig.depthMax = depthMax;
    fconfig.dithering = dithering;
    m_pendingDelay = 0;
    cgif_rgb_addframe(pGIF, &fconfig);
    return true;
}

bool GifEncoder::close(){
    if (pGIF == nullptr) {
        return false;
    }
    cgif_result r = cgif_rgb_close(pGIF);
    pGIF = nullptr;
    m_prevFrame = QImage();
    m_pendingDelay = 0;
    return r == CGIF_OK;
}
