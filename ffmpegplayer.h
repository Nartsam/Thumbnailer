#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H

#include<QLabel>
#include"videoplayer.h"

class FFmpegPlayer:public VideoPlayer{
    Q_OBJECT
public:
    explicit FFmpegPlayer(QObject *parent=nullptr);
    ~FFmpegPlayer();
public: //extends functions
    void play()override;
    void pause()override;
    void stop()override;
    void seek(long long pos)override;
    void next_frame()override;
    void prev_frame()override;
    int state()override;
    bool set_video(const QString &filename)override;

    //下面三个函数只有在start_decoding开始之后获取到的数据才是准确的
    long long duration()override;
    int width()override;
    int height()override;

    int volume()override;
    void set_volume(int volume)override;
    bool mute()override;
    void set_mute(bool muted)override;
    QImage current_image()override;
    double speed()override;
    long long position()override;
    void set_speed(double speed)override;
    void set_position_changed_threshold(int gap_ms)override;
    void set_output_widget(QWidget *output_widget)override;
    void refresh_output_widget_size()override;
    QString video()override;
private:
    void start_decoding();
    void delay(int ms);
    void init();
private:
    QWidget *output_widget;
    QLabel *video_label;
    int mVolume;

    int mPlayState;
    long long mDuration;
    bool mNeedSeek;
    long long mPosition; //current msec
    long long mSeekPos; //second
    int mWidth,mHeight;

    int UpdateMSec; //触发position_changed的阈值
    QString mVideoPath;
    double mPlaySpd;
    QImage currentImage;
    int errCode;
};

#endif // FFMPEGPLAYER_H
