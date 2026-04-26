#ifndef THUMBNAILERDIALOG_H
#define THUMBNAILERDIALOG_H

#include<QDialog>
#include<QStandardItemModel>
#include<QHash>
#include"thumbnailer.h"


namespace Ui{
class ThumbnailerDialog;
}


class VideoPlayer;
class ThumbnailerDialog:public QDialog{
    Q_OBJECT

public:
    enum PlayerList{Player_FFmpeg,Player_PotPlayer,Player_MPlayer};
    //设置默认的播放器
    static void SetDefaultVideoPlayer(PlayerList defPlayer);
public:
    explicit ThumbnailerDialog(QWidget *parent=nullptr,const QStringList &args={});
    ~ThumbnailerDialog();
    void set_arguments(const QStringList &arg_list);
    void show_info(const QString &str); //在底栏显示一行信息
    bool set_video(const QString &video_path);
    bool set_thumbs_dir(const QString &t_dir);
    bool set_thumbs_name(const QString &t_name);
    void display();

    //设置cur_video Edit, thumbs_path Edit, change_thumbs_path Button 是否可编辑
    void set_video_file_widgets_enable(bool flag);
    QString get_thumbnails_path(); //调用thumbnailer的同名函数
    QString get_video_player_name(); //返回当前使用的播放器名称
    QWidget* get_video_output_widget()const; //返回播放视频的那个QWidget

    /*  设置项  */
    void set_exit_after_generate(bool flag);
    void set_use_snapped_image(bool flag);
    void set_hide_window_when_generating(bool flag);
    void set_video_player_by_name(PlayerList player_name);
    void set_print_image_save_path(bool flag);

    /*  全局热键: 通过Win32 RegisterHotKey实现,窗口在后台时也能响应  */
    void set_global_hotkey(const QString &name,Qt::KeyboardModifiers modifiers,Qt::Key key); //按名称注册,如set_global_hotkey("snap",Ctrl,S)
    void clear_global_hotkey(const QString &name); //按名称注销

protected:
    void closeEvent(QCloseEvent* event)override;
    void resizeEvent(QResizeEvent* event)override;
    bool eventFilter(QObject *watched,QEvent *event)override;
    void keyPressEvent(QKeyEvent *event)override;
    void showEvent(QShowEvent *event)override;
    bool nativeEvent(const QByteArray &eventType,void *message,qintptr *result)override; //接收WM_HOTKEY消息

private:
    void init();
    void setup_settings(); //初始化设置项
    //要求spinbox的乘积大于等于snaplist+more,返回0代表检查通过
    int check_spinbox_valid(int more=0);
    //更新右上角 x/y 标签
    void update_thumbs_cnt_label();
    //设置界面组件的启用状态
    void set_ui_state(bool flag);
    //设置 *player 要使用哪个解码器
    void set_video_player(VideoPlayer *new_player);
    //设置 pushButton 的emoji图标
    void set_buttons_icon();

    void set_playpause_button_icon(int state);
    void dispatch_global_hotkey(const QString &name); //根据热键名称分发到对应的槽函数,新增热键时在此添加映射
    void check_hotkey_release(); //轮询检测所有按键是否已松开,松开后才真正触发动作

signals:
    void dialog_resize_signal(QSize size); //窗口大小发生变化时触发该信号,返回窗口大小
    void thumbs_progress_changed(double rate); //生成thumbs的进度,表示为一个0到1之间的浮点数
    void thumbnails_generated(const QString &tpath);
    void trigger_get_thumbnails(int,int,const QVector<long long>&,const QVector<QImage>&);
//    void closed(); //窗口关闭信号

private slots:
    void on_select_pushButton_clicked();
    void on_playpause_pushButton_clicked();
    void on_stop_pushButton_clicked();
    void on_next_frame_pushButton_clicked();
    void on_prev_frame_pushButton_clicked();
    void on_fast_backward_pushButton_clicked();
    void on_fast_forward_pushButton_clicked();
    void on_progress_horizontalSlider_valueChanged(int value);
    void on_progress_horizontalSlider_sliderReleased();
    void on_get_thumbs_pushButton_clicked();
    void on_play_spd_comboBox_currentIndexChanged(int index);
    void on_volume_comboBox_currentIndexChanged(int index);
    void on_snap_pushButton_clicked();
    void item_clicked(QModelIndex index); //ListView中的某项被点击,询问是否删除该项
    void grab_gif_image(); //抓取一张图片,将其添加到gif图片列表中
    void on_clear_thumbs_pushButton_clicked();
    void on_column_spinBox_valueChanged(int arg1);
    void on_row_spinBox_valueChanged(int arg1);
    void on_change_dir_pushButton_clicked();
    void on_thumbs_dir_lineEdit_textChanged(const QString &arg1); //当thumbs_lineEdit显示的内容改变时,同步更新thumbs_dir
    void on_slow_algo_checkBox_clicked(bool checked);
    void on_no_watermark_checkBox_clicked(bool checked);
    void video_changed_slot(const QString &video_path);
    void state_changed_slot(int state);
    void position_changed_slot(long long pos); //单位: 毫秒
    void duration_changed_slot(long long duration); //单位: 毫秒
    void thumbs_progress_changed_slot(double rate); //范围: 0~1
    void on_gif_recording_pushButton_clicked();

private:
    Ui::ThumbnailerDialog *ui;
    Thumbnailer thumbnailer;
    VideoPlayer *player;

    /* GIF 录制相关 */
    bool is_recording_gif; //当前是否正在录制GIF
    QVector<QImage> gif_image_list;
    QTimer *gif_grab_timer;

    /* 手动截图相关 */
    QVector<QImage> snapped_image_list; //用于保存截图
    QSize icon_size; //ListView中预览图的大小
    QStandardItemModel *snaplist_model;

    /* 设置项 */
    bool use_snapped_image; //如果为true,点击snap按钮保存的截图会用于生成缩略图,而不是按照pts_list的时间重新生成
    bool exit_after_generate; //如果为true,按下gene按钮生成图片后程序会退出
    bool hide_window_when_generating; //如果为true,生成缩略图时会隐藏窗口
    bool print_image_save_path; //如果为true,会打印保存图像的路径
    bool no_gui{false};
    /* 全局热键相关 */
    struct HotkeyInfo{int id;unsigned int nativeMod;unsigned int nativeVk;};
    QHash<QString,HotkeyInfo> registeredHotkeys; //名称 -> 热键信息 (用于注册/注销/查询按键状态)
    QHash<int,QString> hotkeyIdToName;           //热键ID -> 名称 (用于nativeEvent中反查是哪个热键触发的)
    int nextHotkeyId{1};                         //自增ID,每注册一个热键分配一个唯一ID给RegisterHotKey
    QString pendingHotkeyAction;                 //等待按键松开后要执行的热键名称
    QTimer *hotkeyReleaseTimer{nullptr};         //定时器,每20ms检测一次按键是否全部松开
    // 默认播放器
    static PlayerList default_video_player;
};

#endif // THUMBNAILERDIALOG_H
