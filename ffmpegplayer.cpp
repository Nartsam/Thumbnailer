#include"ffmpegplayer.h"
#include<QFileInfo>
#include<QElapsedTimer>

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

#define AVFORMAT_OPEN_ERR    -1
#define FIND_STREAM_INFO_ERR -2
#define NO_STREAMS_ERR       -3
#define FIND_DECODER_ERR     -4
#define AVCODEC_OPEN2_ERR    -5
#define ARGUMENTS_INVALID_ERR 6


#include<QtCore/QCoreApplication> //for processEvents()
void FFmpegPlayer::delay(int ms){ //延时,不能直接sleep延时,UI主线程不能直接被阻塞
    QElapsedTimer stopTime;
    stopTime.start();
    while(stopTime.elapsed()<ms){ //stopTime.elapsed()返回从start开始到现在的毫秒数
        QCoreApplication::processEvents();
    }
}

void FFmpegPlayer::init(){
    mSeekPos=0; mNeedSeek=false; //初始Seek位置
    mPlaySpd=1.0; //初始化播放速度
    mPlayState=VideoPlayer::StopState; //初播放状态
    mPosition=0; mDuration=0;
    mWidth=0; mHeight=0;
}

// 给出原始大小，输出适合在Label上显示的大小
QSize scaled_frame_size(int width,int height,int label_w,int label_h){
    if(width<=0||height<=0) return QSize(width,height); //非法输入
    double fact_w=(double)label_w/width;
    double fact_h=(double)label_h/height;
    if(fact_w<fact_h) return QSize((int)(width*fact_w),(int)(height*fact_w));
    return QSize((int)(width*fact_h),(int)(height*fact_h));
}
QSize scaled_frame_size(const QSize &before,const QSize &after){
    return scaled_frame_size(before.width(),before.height(),after.width(),after.height());
}


FFmpegPlayer::FFmpegPlayer(QObject *parent):VideoPlayer(parent){
    output_widget=nullptr; video_label=nullptr;
    init();
}

FFmpegPlayer::~FFmpegPlayer(){
    delete video_label;
    //delete output_widget; can't del.because it may belong other's
}

void FFmpegPlayer::play(){
    int lastState=mPlayState;
    mPlayState=VideoPlayer::PlayState;
    if(lastState!=mPlayState) emit state_changed(mPlayState);
    if(lastState==VideoPlayer::StopState) start_decoding();
}
void FFmpegPlayer::pause(){
    int lastState=mPlayState;
    mPlayState=VideoPlayer::PauseState;
    if(lastState!=mPlayState) emit state_changed(mPlayState);
}
void FFmpegPlayer::stop(){
    //clear
    int lastState=mPlayState;
    mPlayState=VideoPlayer::StopState;
    if(lastState!=mPlayState) emit state_changed(mPlayState);
    if(this->video_label!=nullptr){
        this->video_label->resize(this->output_widget->size());
        this->video_label->setText("Drag Video Here");
    }
}
void FFmpegPlayer::seek(long long pos){
    mSeekPos=pos; mNeedSeek=true;
    //no need emit
}
void FFmpegPlayer::next_frame(){
    qWarning()<<"un-complete function: "<<__func__;
}
void FFmpegPlayer::prev_frame(){
    qWarning()<<"un-complete function: "<<__func__;
}
int FFmpegPlayer::state(){
    return mPlayState;
}
bool FFmpegPlayer::set_video(const QString &filename){
    if(!QFileInfo::exists(filename)&&!filename.isEmpty()) return false;
    mVideoPath=filename;
    mPlayState=VideoPlayer::StopState;
    mDuration=0; mPosition=0; mSeekPos=0; mWidth=0; mHeight=0;
    mNeedSeek=false;
    emit video_changed(mVideoPath);
    return true;
}
long long FFmpegPlayer::duration(){
    return mDuration;
}
int FFmpegPlayer::width(){
    return mWidth;
}
int FFmpegPlayer::height(){
    return mHeight;
}
int FFmpegPlayer::volume(){
    return mVolume;
}
void FFmpegPlayer::set_volume(int volume){
    if(volume>=0&&volume<=100) mVolume=volume;
    else qWarning()<<"invalid volume value:"<<volume;
}
bool FFmpegPlayer::mute(){
    return mVolume==0;
}
void FFmpegPlayer::set_mute(bool muted){
    mVolume=muted?0:30;
}
QImage FFmpegPlayer::current_image(){
    return currentImage;
}

double FFmpegPlayer::speed(){
    return mPlaySpd;
}
long long FFmpegPlayer::position(){
    return mPosition;
}
void FFmpegPlayer::set_speed(double speed){
    if(speed>0) mPlaySpd=speed;
    else qWarning()<<"invalid speed value:"<<speed;
}
void FFmpegPlayer::set_position_changed_threshold(int gap_ms){
    if(gap_ms>=0) UpdateMSec=gap_ms;
}
void FFmpegPlayer::set_output_widget(QWidget *output_widget){
    this->output_widget=output_widget;
    if(video_label!=nullptr) delete video_label;
    video_label=new QLabel(this->output_widget);
    video_label->resize(this->output_widget->size());
    video_label->setStyleSheet("font-size: 24px; font-weight: bold;");// border: 2px solid black;");
    video_label->setAlignment(Qt::AlignCenter);
    video_label->setText("Drag Video Here");
}
void FFmpegPlayer::refresh_output_widget_size(){
    video_label->resize(this->output_widget->size());
}
QString FFmpegPlayer::video(){
    return mVideoPath;
}
void FFmpegPlayer::start_decoding(){
    errCode=0;
    std::string convert_str=mVideoPath.toStdString();
    const char* videoPath=convert_str.c_str();
    unsigned char* buf;
    int isVideo=-1;
    int ret,gotPicture;
    unsigned int i,streamIndex=0;
    const AVCodec *pCodec; //需要声明为const,否则高版本ffmpeg报错
    AVPacket *pAVpkt;
    AVCodecContext *pAVctx;
    AVFrame *pAVframe,*pAVframeRGB;
    AVFormatContext* pFormatCtx;
    struct SwsContext* pSwsCtx;
    pFormatCtx=avformat_alloc_context(); //创建AVFormatContext
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){ //初始化pFormatCtx
        qCritical("Initialize pFormatCtx Fail!");
        errCode=AVFORMAT_OPEN_ERR; stop();
        return;
    }
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){ //获取音视频流数据信息
        avformat_close_input(&pFormatCtx);
        qCritical("获取音视频流数据信息 Fail!");
        errCode=FIND_STREAM_INFO_ERR; stop();
        return;
    }
    for(i=0;i<pFormatCtx->nb_streams;i++){ //找到视频流的索引
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            streamIndex=i; isVideo=0;
            break;
        }
    }
    if(isVideo==-1){ //没有视频流就退出
        avformat_close_input(&pFormatCtx);
        qCritical("No Video Stream Found!");
        errCode=NO_STREAMS_ERR; stop();
        return;
    }
    //计算 frame_rate 视频帧速率,用于延时
    double frame_rate=av_q2d(pFormatCtx->streams[streamIndex]->r_frame_rate);
    if(std::isnan(frame_rate)){
        frame_rate=av_q2d(pFormatCtx->streams[streamIndex]->avg_frame_rate);
    }
    if(frame_rate==0){ //计算FPS失败，通常情况不会触发这个，可以视情况删除 #2023-04-27
        qWarning("Calc FPS Fail!");
    }
    mDuration=pFormatCtx->duration*1000/AV_TIME_BASE; //计算视频毫秒数
    emit duration_changed(mDuration);
    mPosition=0; mSeekPos=0; mNeedSeek=false; //初始时视频从0开始播放
    pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
    //查找解码器
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){
        avcodec_close(pAVctx);
        avformat_close_input(&pFormatCtx);
        qCritical("Find Decoder Fail!");
        errCode=FIND_DECODER_ERR; stop();
        return;
    }
    //初始化pAVctx
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
        avcodec_close(pAVctx);
        avformat_close_input(&pFormatCtx);
        qCritical("Initialize pAVctx Fail!");
        errCode=AVCODEC_OPEN2_ERR; stop();
        return;
    }
    //初始化pAVpkt
    pAVpkt =(AVPacket*)av_malloc(sizeof(AVPacket));
    //初始化数据帧空间
    pAVframe=av_frame_alloc(); pAVframeRGB=av_frame_alloc();
    //创建图像数据存储buf, av_image_get_buffer_size一帧大小
    buf =(unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1));
    av_image_fill_arrays(pAVframeRGB->data,pAVframeRGB->linesize,buf,AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1);
    //初始化pSwsCtx
    pSwsCtx=sws_getContext(pAVctx->width,pAVctx->height,pAVctx->pix_fmt,pAVctx->width,pAVctx->height,AV_PIX_FMT_RGB32,SWS_BICUBIC,NULL,NULL,NULL);
    mWidth=pAVctx->width; mHeight=pAVctx->height;
    long long last_ms=0;
    //循环读取视频数据
    while(true){
        if(mPlayState==VideoPlayer::StopState) break; //播放结束
        if(mNeedSeek){
            long long seekPos=(mSeekPos*AV_TIME_BASE)/1000;
            av_seek_frame(pFormatCtx,-1,seekPos,AVSEEK_FLAG_BACKWARD); //偏移到指定位置再开始解码, AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
            mNeedSeek=false;
        }
        if(mPlayState==VideoPlayer::PauseState){ //暂停
            delay(300);
        }
        else{ //正在播放
            if(av_read_frame(pFormatCtx,pAVpkt)<0){ //已经没有未解码的数据
                break;
            }
            if(pAVpkt->stream_index==(int)streamIndex){ //如果是视频数据
                // 解码一帧视频数据
                // ## This Method is Deprecated. ##
                // ret=avcodec_decode_video2(pAVctx,pAVframe,&gotPicture,pAVpkt);
                ret=avcodec_send_packet(pAVctx,pAVpkt);
                if(ret<0){
                    qWarning("Decode Unsucceed! In %s",__func__);
                    continue;
                }
                ret=avcodec_receive_frame(pAVctx,pAVframe);
                gotPicture=ret?0:1;
                //********************
                if(gotPicture){
                    sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
                    currentImage=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32);
                    this->video_label->resize(this->output_widget->size());
                    this->video_label->setPixmap(QPixmap::fromImage(currentImage.scaled(scaled_frame_size(currentImage.size(),this->video_label->size()),Qt::IgnoreAspectRatio,Qt::SmoothTransformation)));
                    //cur_stamp是以AV TIME BASE为基准的时间,通常是微秒
                    long long cur_stamp=av_rescale_q(pAVframe->pts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                    if(pAVframe->pts==AV_NOPTS_VALUE){ //对于部分avi格式的视频,获取到的pts是负数,此时尝试用dts计算时间戳
                        cur_stamp=av_rescale_q(pAVframe->pkt_dts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                    }
                    mPosition=(cur_stamp*1000)/AV_TIME_BASE;
                    if(qAbs(mPosition-last_ms)>=UpdateMSec){
                        emit position_changed(mPosition);
                        last_ms=mPosition;
                    }
                    delay((1000.0/frame_rate-3)/mPlaySpd); //播放延时
                }
            }
            av_packet_unref(pAVpkt);
        }
    }
    //释放资源
    sws_freeContext(pSwsCtx);
    av_frame_free(&pAVframeRGB);
    av_frame_free(&pAVframe);
    avcodec_close(pAVctx);
    avformat_close_input(&pFormatCtx);
    stop();
    //qDebug()<<"play finish!";
    return;
}
