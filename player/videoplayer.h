#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include<QObject>

class VideoPlayer:public QObject{
    Q_OBJECT
public:
    explicit VideoPlayer(QObject *parent=nullptr);
    virtual ~VideoPlayer();
    enum PlayerState{PlayState,StopState,PauseState};

/*                  必须实现的功能                */
public:
    virtual void play()=0;
    virtual void pause()=0;
    virtual void stop()=0;

    //seek to given position(ms)
    virtual void seek(long long)=0;
    //return playing state
    virtual PlayerState state()=0;
    //获取当前的视频名称
    virtual QString video()=0;
    virtual bool set_video(const QString&)=0;

    virtual double speed()=0;
    virtual void set_speed(double)=0; //set playing speed

    virtual int volume()=0;
    virtual void set_volume(int)=0; //volume: 0~100

    virtual bool mute()=0;
    virtual void set_mute(bool)=0;

    //return video's duration(ms)
    virtual long long duration()=0;

    virtual int width()=0; //return video's width
    virtual int height()=0; //return video's height

    //return current playing position(ms)
    virtual long long position()=0;

    //set & get video playing widget
    virtual void set_output_widget(QWidget*)=0;
    virtual const QWidget* output_widget()=0;

    //当时间差超过阈值(ms)时,才会触发position_changed信号
    virtual void set_position_changed_threshold(int)=0;
    //返回当前的阈值(ms)
    virtual int position_changed_threshold()=0;

    //返回当前正在显示的图像
    virtual QImage current_image()=0;


/*              可以不实现的功能             */
public:
    //定位到当前帧的下一帧并暂停
    virtual void next_frame();
    //定位到当前帧的上一帧并暂停
    virtual void prev_frame();

    virtual void refresh_output_widget_size();
    //返回播放器的名称(如: ffmpeg, potplayer等)
    virtual QString video_player_name();

signals:
    void video_changed(const QString&);
    void state_changed(PlayerState);
    void position_changed(long long); //msec
    void duration_changed(long long); //msec

};

#endif // VIDEOPLAYER_H
