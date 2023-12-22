#ifndef THUMBNAILER_H
#define THUMBNAILER_H

#include<QDialog>
#include<QStandardItemModel>
#include"videoplayer.h"

class Thumbnailer:public QObject{
    Q_OBJECT
public:
    explicit Thumbnailer(QObject *parent=nullptr,const QString &path=QStringLiteral());
    ~Thumbnailer();
    bool set_video(const QString &filename);
    bool set_thumbs_dir(const QString &filename); //给定一个文件,则设定为该文件的同级目录. 路径不存在则设置失败
    bool set_thumbs_name(const QString &filename); //没检查输入合法性
    QString get_thumbnails_path(); //获取thumbs_dir+thumbs_name
    /*
     * 生成视频文件的缩略图(PNG格式)
     * media_path: 视频文件的地址
     * thumbs_name: 生成的图片名称，不包含拓展名，默认和视频同名
     * gene_dir: 生成图片的路径(文件夹)，默认生成在视频同目录下，图片名和视频名相同，若存在重名文件则覆盖
     *          p.s. 若gene_dir是一个文件而不是目录,那么保存图片会失败,但是set_thumbs_dir可以接受一个文件路径
     * ### 需要注意，程序**可能**不能正确解析文件夹名称中含有'.'的路径
     * pts_list: 手动截取的视频位置，时间单位为ms，**没有检查其合法性
    */
    bool get_thumbnails(const QString &media_path,int row,int column,const QString &gene_dir,const QString &tbs_name,const QVector<long long> &plist);
    bool get_thumbnails(int row,int column);
    bool get_thumbnails(int row,int column,const QVector<long long> &plist);
    //截取snap_sec秒位置的图片作为封面,没有检查snap_sec是否超出视频时长
    bool get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec=15);
    bool get_cover(int snap_sec=15);
    bool get_video_info(const QString &media_path,int &duration,int &width,int &height);

signals:
    void thumbs_progress_changed(double rate);
public:
    static bool SlowThumbnailsAlgorithm;
    static bool RemoveThumbnailsMark;
private:
    QString video_path;
    QString thumbs_dir;
    QString thumbs_name;
};


namespace Ui {
class ThumbnailerDialog;
}

class ThumbnailerDialog: public QDialog{
    Q_OBJECT

public:
    explicit ThumbnailerDialog(QWidget *parent = nullptr);
    ~ThumbnailerDialog();
    bool set_video(const QString &video_path);
    bool set_thumbs_dir(const QString &t_dir);
    bool set_thumbs_name(const QString &t_name);
    QString get_thumbnails_path(); //调用thumbnailer的同名函数
    void set_exit_after_generate(bool flag);

private:
    void init();
    int check_spinbox_valid(int more=0); //要求spinbox的乘积大于等于snaplist+more,返回0代表检查通过
    void update_thumbs_cnt_label(); //更新右上角 x/y 标签

private slots:
    void on_select_pushButton_clicked();
    void on_playpause_pushButton_clicked();
    void on_stop_pushButton_clicked();
    void on_progress_horizontalSlider_valueChanged(int value);
    void on_progress_horizontalSlider_sliderReleased();
    void on_get_thumbs_pushButton_clicked();
    void on_play_spd_comboBox_currentIndexChanged(int index);
    void on_volume_comboBox_currentIndexChanged(int index);
    void on_snap_pushButton_clicked();
    void item_clicked(QModelIndex index); //ListView中的某项被点击,询问是否删除该项
    void on_clear_thumbs_pushButton_clicked();
    void on_column_spinBox_valueChanged(int arg1);
    void on_row_spinBox_valueChanged(int arg1);
    void on_change_dir_pushButton_clicked();
    void on_thumbs_dir_lineEdit_textChanged(const QString &arg1); //当thumbs_lineEdit显示的内容改变时,同步更新thumbs_dir
    void on_slow_algo_checkBox_clicked(bool checked);
    void on_no_watermark_checkBox_clicked(bool checked);

protected:
    void closeEvent(QCloseEvent* event)override;
    void resizeEvent(QResizeEvent* event)override;
    bool eventFilter(QObject *watched,QEvent *event)override;

private slots:
    void video_changed_slot(const QString &video_path);
    void state_changed_slot(int state);
    void position_changed_slot(long long pos); //单位: 毫秒
    void duration_changed_slot(long long duration); //单位: 毫秒
    void thumbs_progress_changed_slot(double rate); //范围: 0~1

private:
    Ui::ThumbnailerDialog *ui;
    Thumbnailer thumbnailer;
    VideoPlayer *player;
    QSize icon_size; //ListView中预览图的大小
    QStandardItemModel *snaplist_model;
    bool exit_after_generate; //如果为true,按下gene按钮生成图片后程序会退出
};

#endif // THUMBNAILER_H
