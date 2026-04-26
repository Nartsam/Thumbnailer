#include"mplayer.h"
#include<QFileInfo>
#include<QWidget>
#include<QTime>


QString MPlayer::MPlayerPath="./mplayer/mplayer.exe";

MPlayer::MPlayer(QObject *parent):VideoPlayer{parent}{
    mprocess=new QProcess;
    play_state=PlayerState::StopState;
    output_widget_id=0;
    upd_msec=0; //应该在构造时初始化一次，init中不要初始化
}
MPlayer::~MPlayer(){
}
void MPlayer::set_output_widget(QWidget *output_widget){
    output_widget_id=output_widget->winId();
}
void MPlayer::next_frame(){
    //write_command("frame_step\n"); //bug:写入完暂停失效
}
QString MPlayer::video(){
    return video_path;
}
bool MPlayer::check_set_available(){
    return play_state==PlayerState::PlayState||play_state==PlayerState::PauseState;
}
void MPlayer::update_video_data(){
    if(play_state==PlayerState::PauseState) return;
    write_command(QStringLiteral("get_time_length\n"));
    write_command(QStringLiteral("get_time_pos\n"));
    while(mprocess->canReadLine()){
        QByteArray b(mprocess->readLine());
        b.replace(QByteArray("\n"),QByteArray(""));
        QString s(b);
        if(b.startsWith("ANS_LENGTH")){ //视频总时间
            double tot_time=QStringView(s.mid(11)).toDouble();
            update_duration((long long)(tot_time*1000)); //总时间精确到秒，所以只要整数部分
        }
        else if(b.startsWith("ANS_TIME_POSITION")){ //视频当前时间
            double cur_time=QStringView(s.mid(18)).toDouble();
            update_current_pos((long long)(cur_time*1000));
        }
        // 视频播放进度百分比,和 mprocess->write("get_percent_pos\n"); 配对
        // else if(b.startsWith("ANS_PERCENT_POSITION")){
        //     QString currentPercent=s.mid(21);
        //     qDebug()<<currentPercent<<b;
        // }
        //else qDebug()<<"Got input:"<<s;
    }
}

void MPlayer::init(){
    m_duration=0; cur_msec=0; m_volume=0;
}
void MPlayer::update_duration(long long msec){
    if(msec==m_duration) return;
    m_duration=msec;
    emit duration_changed(msec);
}
void MPlayer::update_current_pos(long long msec){
    static long long last_upd_pos=0;
    cur_msec=msec;
    if(qAbs(last_upd_pos-msec)>upd_msec){
        emit position_changed(msec);
        last_upd_pos=msec;
    }
}
void MPlayer::write_command(const QString &cmd){
    mprocess->write(cmd.toLocal8Bit());
    if(cmd==QStringLiteral("pause\n")){ //刚才写的是pause
        return;
    }
    if(play_state==PlayerState::PauseState){
        mprocess->write("pause\n"); //写入命令会破坏暂停状态,所以要再暂停一遍
    }
}
void MPlayer::play(){
    if(output_widget_id==0||video_path.isEmpty()){
        qCritical()<<"can't play because Win_ID or video is null."; return;
    }
    if(play_state==PlayerState::StopState){
        mprocess->kill(); delete mprocess;
        mprocess=new QProcess;
        mprocess->setProcessChannelMode(QProcess::MergedChannels);
        mprocess->setProgram(MPlayerPath);
        QStringList args;
        if(QFileInfo(video_path).suffix()=="ts"){
            args<<"-demuxer lavf"; //用Mplayer播放.ts文件时,视频太快,强制使用lavf视音频分离器使之正常
        }
        args<<video_path;
        args<<"-slave"; //"-slave"使其不再接受键盘事件,而接受 \n 结束的命令控制
        args<<"-quiet"; //尽可能的不打印播放信息
        args<<"-zoom"; //视频居中,四周黑条,全屏播放
        args<<"-wid"<<QString::number(output_widget_id); //"-wid <窗口标识>"让MPlayer依附于那个窗口,这样视频播放的时候就在这个部件里播放,相当于给他固定起来
        mprocess->setArguments(args);
        connect(mprocess,&QProcess::readyReadStandardOutput,this,&MPlayer::update_video_data);
        mprocess->start();
    }
    else if(play_state==PlayerState::PauseState){
        write_command(QStringLiteral("pause\n")); //pause可以切换播放/暂停状态
    }
    //本来就是play状态,就不用动了
    if(play_state!=PlayerState::PlayState){
        play_state=PlayerState::PlayState;
        emit state_changed(PlayerState::PlayState);
        update_video_data(); //手动调用一次update以免从暂停切换过来后mplayer没有输出导致无法触发upd
    }
}
void MPlayer::pause(){
    if(play_state==PlayerState::StopState) return; //无效操作
    if(play_state!=PlayerState::PauseState){
        write_command(QStringLiteral("pause\n"));
        play_state=PlayerState::PauseState;
        emit state_changed(PlayerState::PauseState);
    }
}
void MPlayer::stop(){ //bug: 当mplayer正常播放完视频后无法将状态转换为stop
    if(play_state!=PlayerState::StopState){
        write_command(QStringLiteral("quit\n"));
        play_state=PlayerState::StopState;
        emit state_changed(PlayerState::StopState);
    }
}
void MPlayer::seek(long long pos){
    if(!check_set_available()) return;
    double sk=(double)pos/1000.0; //转换成秒数
    write_command(QString("seek "+QString::number(sk)+" 2\n"));
    update_current_pos(pos); //因为暂停状态下的seek不会更新cur_pos,这里先手动更新一下
}
VideoPlayer::PlayerState MPlayer::state(){
    return play_state;
}
bool MPlayer::set_video(const QString &filename){
    if(!QFileInfo::exists(filename)&&!filename.isEmpty()) return false;
    stop();
    video_path=filename;
    init();
    emit video_changed(video_path);
    return true;
}
long long MPlayer::duration(){
    return m_duration;
}
int MPlayer::width(){
    qDebug()<<"MPlayer::width() is incomplete."; return 0;
}
int MPlayer::height(){
    qDebug()<<"MPlayer::height() is incomplete."; return 0;
}
int MPlayer::volume(){
    return m_volume;
}
void MPlayer::set_volume(int volume){
    if(!check_set_available()) return;
    if(volume>=0&&volume<=100&&m_volume!=volume){
        m_volume=volume;
        write_command(QString("volume "+QString::number(volume)+" 2\n"));
    }
}
bool MPlayer::mute(){
    return m_volume==0;
}
void MPlayer::set_mute(bool muted){
    if(!check_set_available()) return;
    if(muted) set_volume(0);
    else if(m_volume==0) set_volume(80);
}
double MPlayer::speed(){
    return m_speed;
}
void MPlayer::set_speed(double speed){
    if(!check_set_available()) return;
    if(speed>0&&speed<10){
        m_speed=speed;
        write_command(QString("speed_set "+QString::number(speed)+"\n"));
    }
}
long long MPlayer::position(){
    return cur_msec;
}
void MPlayer::set_position_changed_threshold(int gap_ms){
    if(gap_ms>=0) upd_msec=gap_ms;
}
int MPlayer::position_changed_threshold(){
    return upd_msec;
}
