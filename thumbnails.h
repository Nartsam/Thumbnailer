#ifndef THUMBNAILS_H
#define THUMBNAILS_H

#include<QDialog>
#include<QLabel>
#include<QVector>
#include<QStringListModel>
#include<QStandardItemModel>
extern "C"{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswscale/swscale.h>
#include<libavdevice/avdevice.h>
#include<libavformat/version.h>
#include<libavutil/time.h>
#include<libavutil/mathematics.h>
#include<libavutil/imgutils.h>
}

class Thumbnails{
public:
    explicit Thumbnails(const QString &path=QStringLiteral());
    ~Thumbnails();
    bool set_video(const QString &filename);
    bool set_thumbs_dir(const QString &filename);
    //没检查输入合法性
    bool set_thumbs_name(const QString &filename);
    /*
     * 生成视频文件的缩略图(PNG格式)
     * media_path: 视频文件的地址
     * thumbs_name: 生成的图片名称，不包含拓展名，默认和视频同名
     * gene_dir: 生成图片的路径(文件夹)，默认生成在视频同目录下，图片名和视频名相同，若存在重名文件则覆盖
     *          p.s. 若gene_dir是一个文件而不是目录,那么保存图片会失败,但是set_thumbs_dir可以接受一个文件路径
     * ### 需要注意，程序**可能**不能正确解析文件夹名称中含有'.'的路径
     * pts_list: 手动截取的视频位置，时间单位为ms，**没有检查其合法性
    */
    static int get_thumbnails(const QString &media_path,int row,int column,const QString &gene_dir,const QString &tbs_name,const QVector<long long> &plist);
    int get_thumbnails(int row,int column);
    int get_thumbnails(int row,int column,const QVector<long long> &plist);
    static int get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec=15);
    int get_cover(int snap_sec=15);
    QString get_thumbnails_path();

private:
    QString video_path;
    QString thumbs_dir;
    QString thumbs_name;
    static bool SlowThumbnailsAlgorithm;
    static bool RemoveThumbnailsMark;
};


//class FFmpegPlayer{
//public:
//    void play();
//    void stop();
//    void pause();
    
//private:
//    QLabel *mWidget; //video output widget
//};


namespace Ui {
class ThumbnailsDialog;
}

class ThumbnailsDialog: public QDialog{
    Q_OBJECT
    
public:
    explicit ThumbnailsDialog(QWidget *parent = nullptr);
    ~ThumbnailsDialog();
    bool set_video(const QString &video_path);
    bool set_thumbs_dir(const QString &t_dir);
    bool set_thumbs_name(const QString &t_name);
    void set_exit_after_generate(bool flag);
    QString get_thumbnails_path();

private slots:
    void on_select_pushButton_clicked();
    void on_playpause_pushButton_clicked();
    void on_stop_pushButton_clicked();
    void on_progress_horizontalSlider_valueChanged(int value);
    void on_progress_horizontalSlider_sliderReleased();
    void on_get_thumbs_pushButton_clicked();
    void on_play_spd_comboBox_currentIndexChanged(int index);
    void on_snap_pushButton_clicked();
    void item_clicked(QModelIndex index);
    void on_del_thumb_pushButton_clicked();
    void on_clear_thumbs_pushButton_clicked();
    void on_column_spinBox_valueChanged(int arg1);
    void on_row_spinBox_valueChanged(int arg1);
    void on_change_dir_pushButton_clicked();
    //当thumbs_lineEdit显示的内容改变时,同步更新thumbs_dir
    void on_thumbs_dir_lineEdit_textChanged(const QString &arg1);
    
protected:
    void closeEvent(QCloseEvent* event)override;
    void resizeEvent(QResizeEvent* event)override;
    bool eventFilter(QObject *watched,QEvent *event)override;

private:
    void init();
    int playVideo();
    void update_listview();
    int check_spinbox_valid();

private:
    Ui::ThumbnailsDialog *ui;
    Thumbnails thumbnails;
    int mPlayState;
    double mPlaySpd;
    long long mSeekPos; //second
    bool mNeedSeek;
    bool mSnapped;
    QString mVideoPath;
    QVector<double> mPlaySpdList;
    QVector<int> mPlayVolList;
    QVector<long long> mSnappedList;
    QStandardItemModel *StringItemModel;
    //如果为true,当按下gene按钮生成图片后,程序会退出
    bool exit_after_generate;
};


#endif // THUMBNAILS_H
