#ifndef THUMBENGINE_H
#define THUMBENGINE_H

#include<QObject>
#include<QImage>
#include"VideoInfo.h"

class ThumbEngine:public QObject{
    Q_OBJECT
public:
    explicit ThumbEngine(QObject *parent=nullptr);
    ~ThumbEngine();
    /*
     * 生成视频文件的缩略图, 返回QImage
     * media_path: 视频文件的地址
     * pts_list: 手动截取的视频位置，时间单位为ms，**没有检查其合法性
     * 该函数可能会抛出异常
    */
    QImage get_thumbnails(const QString &media_path,int row,int column,const QVector<long long> &plist,bool slow_algo,bool watermark);
    //获取媒体信息(长宽,时长)
    VideoInfo get_video_info(const QString &media_path);

signals:
    void thumbs_progress_changed(double rate); //生成thumbs的进度,表示为一个0到1之间的浮点数
public:
    int ThumbsWidthLimit;
    int ThumbsHeightLimit;
};

#endif // THUMBENGINE_H
