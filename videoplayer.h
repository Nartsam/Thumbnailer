#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include<QImage>
#include<QObject>

class VideoPlayer:public QObject{
    Q_OBJECT
public:
    explicit VideoPlayer(QObject *parent=nullptr);
    virtual ~VideoPlayer();
public:
    const static int PlayState=1;
    const static int StopState=0;
    const static int PauseState=2;
public:
    virtual void play()=0;
    virtual void pause()=0;
    virtual void stop()=0;
    virtual void seek(long long)=0;
    virtual int state()=0;
    virtual bool set_video(const QString&)=0;
    virtual long long duration()=0;
    virtual int width()=0;
    virtual int height()=0;
    virtual int volume()=0;
    virtual void set_volume(int)=0;
    virtual bool mute()=0;
    virtual void set_mute(bool)=0;
    virtual QImage current_image()=0;
    virtual double speed()=0;
    virtual void set_speed(double)=0;
    virtual void set_output_widget(QWidget*)=0;
    virtual long long position()=0; //return msec
    //当时间差超过阈值(ms)时,才会触发position_changed信号
    virtual void set_position_changed_threshold(int)=0;
    virtual QString video()=0; //获取当前的视频名称
    virtual void next_frame();
    virtual void prev_frame();
    virtual void refresh_output_widget_size(); //可以不实现

signals:
    void video_changed(const QString&);
    void state_changed(int);
    void position_changed(long long); //m-second
    void duration_changed(long long); //second
private:

};

#endif // VIDEOPLAYER_H
