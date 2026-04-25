#ifndef THUMBNAILER_H
#define THUMBNAILER_H

#include<QObject>
#include<QImage>

//一个小型结构体,用于返回视频文件的相关信息
struct VideoInfo{
    int width,height;
    long long duration;
    double fps;
};


class Thumbnailer:public QObject{
    Q_OBJECT
public:
    explicit Thumbnailer(QObject *parent=nullptr,const QString &path=QStringLiteral());
    ~Thumbnailer();

    //不能设定为不存在的文件,但是可以置空
    bool set_video(const QString &filename);
    QString get_video_path();

    //给定一个文件,则设定为该文件的同级目录. 路径不存在则设置失败
    bool set_thumbs_dir(const QString &filename);
    //没检查输入合法性,自动抹除其拓展名
    bool set_thumbs_name(const QString &filename);
    //获取thumbs_dir+sptor+thumbs_name
    QString get_thumbnails_path(char sptor=0);
    //给定一个QImage列表，从有效的图像中找出最小的QImage的大小
    QSize calc_size_from_image_list(const QVector<QImage> &v,int maxw=MAX_THUMBS_WIDTH,int maxh=MAX_THUMBS_HEIGHT)const;

    //获取媒体文件的信息(长宽,时长等),若width或height有一项为-1就表示获取失败
    static VideoInfo get_video_info(const QString &media_path);

public slots:
    /*
     * 生成视频文件的缩略图(PNG格式)
     * @media_path: 视频文件的地址
     * @thumbs_name: 生成的图片名称，$不包含拓展名$，默认和视频同名
     * @gene_dir: 生成图片的路径(文件夹)，默认生成在视频同目录下，图片名和视频名相同，若存在重名文件则覆盖
     *          p.s. 若gene_dir是一个文件而不是目录,那么保存图片会失败,但是set_thumbs_dir可以接受一个文件路径
     * *** ~~需要注意，程序**可能**不能正确解析文件夹名称中含有'.'的路径~~
     * @pts_list: 手动截取的视频位置，时间单位为ms，**没有检查其合法性
     * @img_list: img_list[i]等价于pts_list[i]时的图像,若存在则不会再去生成. 不要求两个列表大小相等
     * *** ~~以下函数可能会被链接,尽量不要修改它们的参数列表~~
    */
    bool get_thumbnails(const QString &media_path,int row,int column,
                        const QString &gene_dir,const QString &tbs_name,
                        const QVector<long long> &plist,const QVector<QImage> &imglist
                        );
    //此函数只会将小图保存至reslist中,不会写入本地文件 (2025-07-24 将reslist改为指针)
    bool get_thumbnails(const QString &media_path,int row,int column,
                        const QVector<long long> &plist,const QVector<QImage> &imglist,
                        QVector<QImage>* reslist
                        );
    bool get_thumbnails(int row,int column);
    bool get_thumbnails(int row,int column,const QVector<long long> &plist,const QVector<QImage> &imglist);
    //截取snap_sec秒位置的图片作为封面,没有检查snap_sec是否超出视频时长
    bool get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec=15);
    bool get_cover(int snap_sec=15);

signals:
    void thumbs_progress_changed(double rate); //生成thumbs的进度,表示为一个0到1之间的浮点数
    void thumbnails_generated(const QString &tpath);

public:
    bool SlowThumbnailsAlgorithm;
    bool RemoveThumbnailsMark;
    static bool DefaultSlowThumbnailsAlgorithm;
    static bool DefaultRemoveThumbnailsMark;
    static int ThumbsLimit(); //获取每行/列最多能画多少个缩略图

    //因为get_thumbnails函数与QTread进行了链接，所以通过一般途径得不到它的返回值
    //规定该函数在返回时同步修改这个变量的值,以获取其返回值,该变量在其他时刻的值可被任意修改,无意义
    int get_thumbnails_result;

private:
    static int MaxThumbsLimit; //每行/列最多能画多少个缩略图
    static int MAX_THUMBS_WIDTH;
    static int MAX_THUMBS_HEIGHT;

private:
    QString video_path;
    QString thumbs_dir;
    QString thumbs_name;
};


#endif // THUMBNAILER_H
