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
private:
    void start_decoding();
    void delay(int ms);
    void init();
private:
    QWidget *ffmpeg_output_widget;
    QLabel *video_label;
    int UpdateMSec; //触发position_changed的阈值
    QString mVideoPath;
    QImage currentImage;
    int errCode;
    // 和当前播放配置有关的参数
    int mVolume;
    PlayerState mPlayState;
    long long mDuration,mPosition,mSeekPos; //单位：ms
    bool mNeedSeek;
    int mWidth,mHeight;
    double mPlaySpd;
};

#endif // FFMPEGPLAYER_H
