#ifndef GIFENCODER_H
#define GIFENCODER_H

#include<QObject>
#include"cgif.h"

class GifEncoder:public QObject{
    Q_OBJECT
public:
    explicit GifEncoder(QObject *parent = nullptr);

public slots:
    bool open(QString filename, int width, int height);
    bool push(QImage &image, int delayTime);
    bool close();

private:
    CGIFrgb *pGIF = nullptr;
};



#endif // GIFENCODER_H
