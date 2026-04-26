#ifndef POTPLAYER_H
#define POTPLAYER_H

#include"videoplayer.h"
#include<QProcess>

class PotPlayer:public VideoPlayer{
public:
    explicit PotPlayer(QObject *parent=nullptr);
    ~PotPlayer();

public: //extends functions
    void play()override;
    void pause()override;
    void stop()override;
    void seek(long long pos)override;
    // void next_frame()override;
    // void prev_frame()override;
    PlayerState state()override;
    bool set_video(const QString &filename)override;

    //下面三个函数只有在start_decoding开始之后获取到的数据才是准确的
    long long duration()override;
    int width()override;
    int height()override;

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
    const QWidget* output_widget()override;
    void refresh_output_widget_size()override;
    QString video()override;
    QImage current_image()override;
    QString video_player_name()override;

private:
    unsigned long long start_potplayer(const QString &vpath);
    void delay(int ms);
    void active_potplayer(int delay_ms=0); //尝试将焦点置于PotPlayer窗口
    bool make_sure_potplayer_exists();
public:
    static QString PotPlayerPath;
private:
    QWidget *potplayer_output_widget;
    QProcess *process;
    QString video_path;
    PlayerState current_state;
    unsigned long long potplayer_wid; //Type: WId
    int position_changed_threshold_val;
};

#endif // POTPLAYER_H
