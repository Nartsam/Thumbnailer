#ifndef GIFENCODER_H
#define GIFENCODER_H

#include<QObject>
#include"cgif.h"

class GifEncoder:public QObject{
    Q_OBJECT
public:
    explicit GifEncoder(QObject *parent=nullptr);

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
};



#endif // GIFENCODER_H
