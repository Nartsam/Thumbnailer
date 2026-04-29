#ifndef GIFENCODER_H
#define GIFENCODER_H

#include<QObject>
#include<QImage>
#include"cgif.h"

class GifEncoder:public QObject{
    Q_OBJECT
public:
    explicit GifEncoder(QObject *parent=nullptr);

    /*
        压缩质量: 1~10, 默认 10 (与原始行为完全一致)
        值越小文件越小, 色彩保真度越低; 在 open() 前调用
    */
    void setQuality(int quality);

public slots:
    /*
        开始写入filename(本地不存在就创建这个文件，本地存在就重新覆盖)
    */
    bool open(QString filename,int width,int height);
    /*
        @image会被缩放到open()时设置的width和height大小, 该张图片在最终的 GIF 中显示 @delayTime 毫秒
    */
    bool push(QImage &image,int delayTime);
    bool close();

private:
    CGIFrgb *pGIF = nullptr;
    int gifWidth = 0;
    int gifHeight = 0;
    int m_quality = 10;
    QImage m_prevFrame;      // 帧跳过: 缓存上一帧用于相似度比较
    int m_pendingDelay = 0;  // 帧跳过: 被跳过帧的累计延时(毫秒)
};



#endif // GIFENCODER_H
