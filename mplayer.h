#ifndef MPLAYER_H
#define MPLAYER_H

#include"videoplayer.h"
#include<QProcess>

class MPlayer:public VideoPlayer{
public:
    explicit MPlayer(QObject *parent=nullptr);
    ~MPlayer();
public: //extends functions
    void play()override;
    void pause()override;
    void stop()override;
    void seek(long long pos)override;
    PlayerState state()override;
    bool set_video(const QString &filename)override;
    long long duration()override; //在play()开始后获取到的数据才是有效的
    int width()override; //没有实现
    int height()override; //没有实现
    int volume()override;
    void set_volume(int volume)override;
    bool mute()override;
    void set_mute(bool muted)override;
    double speed()override;
    long long position()override;
    void set_speed(double speed)override;
    void set_position_changed_threshold(int gap_ms)override;
    int position_changed_threshold()override;
    void set_output_widget(QWidget *output_widget)override;
    void next_frame()override;
    QString video()override;

private:
    bool check_set_available(); //检查当前状态能否使用set_xxx(),防止在StopState时被设置
    void init();
    void update_duration(long long msec);
    void update_current_pos(long long msec);
    void write_command(const QString &cmd); //只要写入mplayer就会继续播放,覆盖之前的pause状态,因此在该函数中统一处理写入操作

private slots:
    void update_video_data();

private:
    QProcess *mprocess;
    long long m_duration; //单位：ms
    long long cur_msec,upd_msec; //当前播放到多少毫秒了; 触发position_changed的阈值
    PlayerState play_state;
    int m_volume;
    double m_speed;
    QString video_path;
    unsigned long long output_widget_id; //output_widget的窗口ID,类型: WId

    static QString MPlayerPath;
};

/*
 * MPlayer Commands List:
 * https://www.cnblogs.com/hnrainll/archive/2011/04/26/2028817.html
*/


#endif // MPLAYER_H
