#include"thumbnails.h"
#include"ui_thumbnails.h"
#include<QDebug>
#include<QElapsedTimer>
#include<QPainter>
#include<QCloseEvent>
#include<QPixmap>
#include<QPoint>
#include<QFileDialog>
#include<QMessageBox>
#include<QSlider>
#include<QComboBox>
#include<QMimeData>
#include<cstring>
#include<queue>

// 此窗口**设置**了在关闭时释放资源

#define AVFORMAT_OPEN_ERR    -1
#define FIND_STREAM_INFO_ERR -2
#define NO_STREAMS_ERR       -3
#define FIND_DECODER_ERR     -4
#define AVCODEC_OPEN2_ERR    -5
#define ARGUMENTS_INVALID_ERR 6
#define SAVE_IMAGE_ERR       -7

#define PLAY_STATE 1
#define FINISH_STATE 0
#define PAUSE_STATE 2
// 进度条的精度的倒数(秒): e.g.如果为10,则精度为0.1秒
#define SliderPrecision 10

#define THUMBS_WIDTH 1024
#define THUMBS_HEIGHT 576
#define THUMBS_LIMIT 9


bool Thumbnails::SlowThumbnailsAlgorithm=false;
bool Thumbnails::RemoveThumbnailsMark=false;

bool video_exists(const QString &path){
    if(path.isEmpty()) return false;
    QFileInfo fileInfo(path);
    if(fileInfo.isFile()) return true;
    return false;
}
//延时,不能直接sleep延时,UI主线程不能直接被阻塞
static void delay(int ms){
    QElapsedTimer stopTime;
    stopTime.start();
    while(stopTime.elapsed()<ms){ //stopTime.elapsed()返回从start开始到现在的毫秒数
        QCoreApplication::processEvents();
    }
}
// 给出原始大小，输出适合在Label上显示的大小
QSize scaled_frame_size(int width,int height,int label_w,int label_h){
    if(width==0||height==0) return QSize(width,height); //除数不能为0
//    if(label_w==-1||label_h==-1){
//        label_w=ui->video_label->width(); label_h=ui->video_label->height();
//    }
    double fact_w=(double)label_w/width;
    double fact_h=(double)label_h/height;
    if(fact_w<fact_h) return QSize((int)(width*fact_w),(int)(height*fact_w));
    return QSize((int)(width*fact_h),(int)(height*fact_h));
}
QSize scaled_frame_size(const QSize &before, const QSize &after){
    return scaled_frame_size(before.width(),before.height(),after.width(),after.height());
}
// ********************* Thumbnails ************************
Thumbnails::Thumbnails(const QString &path){
    set_video(path);
}
Thumbnails::~Thumbnails(){

}
bool Thumbnails::set_video(const QString &filename){
    if(!video_exists(filename)&&!filename.isEmpty()) return false;
    video_path=filename;
    //update thumbs_dir & thumbs_name
    if(!filename.isEmpty()){
        QFileInfo sinfo(filename);
        thumbs_dir=sinfo.absolutePath()+"/";
        thumbs_name=sinfo.completeBaseName();
    }
    else{thumbs_dir.clear(); thumbs_name.clear();}
    return true;
}
bool Thumbnails::set_thumbs_dir(const QString &filename){
    if(!QFileInfo::exists(filename)) return false;
    QFileInfo sinfo(filename);
    if(sinfo.isFile()) thumbs_dir=sinfo.absolutePath()+'/';
    else thumbs_dir=sinfo.absoluteFilePath()+'/';
    return true;
}
//该函数没有对输入的合法性进行检查
bool Thumbnails::set_thumbs_name(const QString &filename){
    thumbs_name=filename; return true;
}

int Thumbnails::get_thumbnails(int row,int column){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,QVector<long long>());
}
int Thumbnails::get_thumbnails(int row,int column,const QVector<long long> &plist){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,plist);
}
int Thumbnails::get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec){
    QVector<long long> v; v.push_back(snap_sec*AV_TIME_BASE);
    RemoveThumbnailsMark=true;
    int res=get_thumbnails(media_path,1,1,gene_dir,cover_name,v);
    RemoveThumbnailsMark=false;
    return res;
}
int Thumbnails::get_cover(int snap_sec){
    return get_cover(video_path,thumbs_dir,thumbs_name,snap_sec);
}
QString Thumbnails::get_thumbnails_path(){ //没考虑 \ 路径的情况(应该是不用考虑)
    if(thumbs_dir.endsWith('/')) return thumbs_dir+thumbs_name+QStringLiteral(".png");
    return thumbs_dir+QStringLiteral("/")+thumbs_name+QStringLiteral(".png");
}



int Thumbnails::get_thumbnails(const QString &media_path,int row,int column,const QString &gene_dir,const QString &tbs_name,const QVector<long long> &plist){
    // row 和 column 只要有一个是0，就根据视频时长重新计算 row 和 column
    if(row<0||row>THUMBS_LIMIT||column<0||column>THUMBS_LIMIT||plist.size()>row*column){
        qCritical("Get Thumbnails: Argumrnts Invalid! row:%d col:%d plist:%lld.",row,column,plist.size());
        return ARGUMENTS_INVALID_ERR;
    }
    if(media_path.isEmpty()){
        qCritical("Get Thumbnails: Empty media file path."); return AVFORMAT_OPEN_ERR;
    }
    if(gene_dir.isEmpty()||tbs_name.isEmpty()){
        qCritical("Get Thumbnails: GenerateDir or ThumbsName is empty."); return ARGUMENTS_INVALID_ERR;
    }
    if(!QFileInfo::exists(gene_dir)){
        qCritical("Get Thumbnails: GenerateDir is not exist."); return ARGUMENTS_INVALID_ERR;
    }
    //********************************** 合法性检查完成 *************************************//
    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
    int ret,isVideo=0;
    unsigned int i,streamIndex=0;
    AVFormatContext* pFormatCtx=avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL) != 0){
        qCritical("Get Thumbnails: Initialize pFormatCtx Fail!");
        return AVFORMAT_OPEN_ERR;
    }
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        qCritical("Get Thumbnails: 获取音视频流数据信息 Fail!");
        avformat_close_input(&pFormatCtx); return FIND_STREAM_INFO_ERR;
    }
    for(i=0;i<pFormatCtx->nb_streams;i++){
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            streamIndex=i; isVideo=1; break;
        }
    }
    if(!isVideo){
        qCritical("Get Thumbnails: No Video Stream Found!");
        avformat_close_input(&pFormatCtx); return NO_STREAMS_ERR;
    }
    long long duration=pFormatCtx->duration; //计算视频秒数(us)
    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
    //查找解码器
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){
        avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
        qCritical("Get Thumbnails: Find Decoder Fail!");
        return FIND_DECODER_ERR;
    }
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
        avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
        qCritical("Get Thumbnails: Initialize pAVctx Fail!");
        return AVCODEC_OPEN2_ERR;
    }
    AVPacket *pAVpkt=(AVPacket*)av_malloc(sizeof(AVPacket));
    AVFrame *pAVframe=av_frame_alloc(),*pAVframeRGB=av_frame_alloc();
    unsigned char *buf=(unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1));
    av_image_fill_arrays(pAVframeRGB->data,pAVframeRGB->linesize,buf,AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1);
    struct SwsContext *pSwsCtx=sws_getContext(pAVctx->width,pAVctx->height,pAVctx->pix_fmt,pAVctx->width,pAVctx->height,AV_PIX_FMT_RGB32,SWS_BICUBIC,NULL,NULL,NULL);
    // *** 计算截取时间戳 ***
    QVector<long long> pts_list(plist);
    std::priority_queue<std::pair<long long,int> > gap_time_queue;
    //若row或column有一项是0，根据视频时长决定其大小
    if(!row||!column){
        if(duration/AV_TIME_BASE<300) row=4,column=4; //less than 5 min
        else if(duration/AV_TIME_BASE<3600) row=6,column=5; //less than 1 hour
        else row=8,column=7;
    }
    long long time_step=(long long)((double)duration/(row*column+1)); //每一段的时间间隔
    for(long long i=1,pos=time_step;i<=row*column;++i,pos+=time_step){
        long long gap_time=INT64_MAX;
        for(auto t:pts_list) gap_time=qMin(gap_time,abs(t-pos));
        gap_time_queue.push(std::make_pair(gap_time,(int)i));
    }
    while(pts_list.size()<column*row){ //选计算的和手动选的差距最大的
        auto p=gap_time_queue.top(); gap_time_queue.pop();
        pts_list.push_back(time_step*p.second);
    }
    std::sort(pts_list.begin(),pts_list.end());
    // 计算每张小图的大小
    QSize thumbs_size(pAVctx->width,pAVctx->height);
    if(pAVctx->width>THUMBS_WIDTH||pAVctx->height>THUMBS_HEIGHT){ //超过THUMBS_WIDTH*THUMBS_HEIGHT就缩放
        thumbs_size=scaled_frame_size(thumbs_size,QSize(THUMBS_WIDTH,THUMBS_HEIGHT));
    }
    // 最终的大图
    QImage result_img(thumbs_size.width()*column,thumbs_size.height()*row,QImage::Format_RGB32);
    QPainter result_painter(&result_img);
    //原始Thumbnails算法
    if(!SlowThumbnailsAlgorithm){
        for(int n=0;n<row;++n){
            for(int m=0;m<column;++m){
                long long time_stamp=pts_list[n*column+m];
                av_seek_frame(pFormatCtx,-1,time_stamp,AVSEEK_FLAG_BACKWARD); //AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
                int error_code=0;
                while(1){
                    if(av_read_frame(pFormatCtx,pAVpkt)<0){error_code=1; break;}
                    if(pAVpkt->stream_index==(int)streamIndex){
                        ret=avcodec_send_packet(pAVctx,pAVpkt);
                        if(ret<0){
                            if(error_code==0) error_code=2;
                            continue;
                        }
                        if(avcodec_receive_frame(pAVctx,pAVframe)==0){
                            long long current_ms=av_rescale_q(pAVframe->pts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                            if(current_ms<time_stamp) continue;
                            sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
                            QImage img=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32).scaled(thumbs_size);
                            if(!RemoveThumbnailsMark){
                                QPainter img_painter(&img);
                                img_painter.setFont(QFont("Arial",img.height()/15));
                                img_painter.setPen(QPen(Qt::white));
                                img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addSecs(time_stamp/AV_TIME_BASE).toString());
                            }
                            result_painter.drawImage(m*thumbs_size.width(),n*thumbs_size.height(),img);
                            error_code=0;
                            break;
                        }
                    }
                    av_packet_unref(pAVpkt);
                }
                if(error_code){
                    qWarning("Get Thumbnails: Get %dst Thumbs Fail! ErrCode %d.",n*column+m+1,error_code);
                }
            }
        }
    }
    else{
        int time_pos=0,m=0,n=0;
        while(av_read_frame(pFormatCtx,pAVpkt)>=0){
            if(pAVpkt->stream_index==(int)streamIndex){
                if(avcodec_send_packet(pAVctx,pAVpkt)<0) continue;
                if(avcodec_receive_frame(pAVctx,pAVframe)==0){
                    long long cur_ms=av_rescale_q(pAVframe->pts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                    if(cur_ms>=pts_list[time_pos]){
                        sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
                        QImage img=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32).scaled(thumbs_size);
                        if(!RemoveThumbnailsMark){
                            QPainter img_painter(&img);
                            img_painter.setFont(QFont("Arial",img.height()/15));
                            img_painter.setPen(QPen(Qt::white));
                            img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addSecs(pts_list[time_pos]/AV_TIME_BASE).toString());
                        }
                        result_painter.drawImage(m*thumbs_size.width(),n*thumbs_size.height(),img);
                        ++time_pos; if(time_pos>=(int)pts_list.size()) break;
                        ++m; if(m>=column) ++n,m=0;
                    }
                }
            }
            av_packet_unref(pAVpkt);
        }
    }
    //释放资源
    sws_freeContext(pSwsCtx);
    av_frame_free(&pAVframeRGB); av_frame_free(&pAVframe);
    avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
    // Save Picture
    QString dist_path(gene_dir);
    if(dist_path.contains('/')&&!dist_path.endsWith('/')) dist_path.append('/');
    if(dist_path.contains('\\')&&!dist_path.endsWith('\\')) dist_path.append('\\');
    dist_path+=(tbs_name+QStringLiteral(".png"));
    if(!result_img.save(dist_path,"PNG")){
        qCritical()<<"Get Thumbnails: Save Image at "<<dist_path<<" Fail!";
        return SAVE_IMAGE_ERR;
    }
    qDebug()<<"Debug: Saved Image at:"<<dist_path;
    return 0;
}



// ******************** ThumbnailsDialog *************************
ThumbnailsDialog::ThumbnailsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ThumbnailsDialog){
    ui->setupUi(this);
    ui->video_label->installEventFilter(this);
    ui->video_label->setAcceptDrops(true);
    setWindowFlags(Qt::WindowMinimizeButtonHint|Qt::WindowMaximizeButtonHint|Qt::WindowCloseButtonHint);
    //setAttribute(Qt::WA_DeleteOnClose); //关闭窗口时释放资源
    init(); //初始化
}
ThumbnailsDialog::~ThumbnailsDialog(){
    delete ui;
}
void ThumbnailsDialog::init(){
    mSeekPos=0; mNeedSeek=false; //初始Seek位置
    mSnapped=false;
    mPlaySpd=1.0; //初始化播放速度
    mPlayState=FINISH_STATE; //初播放状态
    mSnappedList.clear();
    ui->video_label->setGeometry(0,0,ui->widget->width(),ui->widget->height()); //初始化VideoLabel
    ui->row_spinBox->setRange(1,THUMBS_LIMIT);
    ui->column_spinBox->setRange(1,THUMBS_LIMIT);
    ui->row_spinBox->setValue(3);
    ui->column_spinBox->setValue(4); //初始3*4
    //添加播放速度下拉选择
    double play_spd_arr[]={0.5,0.8,1,1.2,1.5,2,4};
    mPlaySpdList.clear();
    QStringList plsySpdItems;
    for(double i:play_spd_arr){
        mPlaySpdList.push_back(i);
        plsySpdItems<<QString::number(i,'f',1);
    }
    ui->play_spd_comboBox->setToolTip("播放速度");
    ui->play_spd_comboBox->addItems(plsySpdItems); //添加倍速选项
    ui->play_spd_comboBox->setCurrentIndex(2); //设置默认速度1
    ui->play_spd_comboBox->setEditable(false); //设置为不可编辑
    //添加音量下拉选择
    int play_vol_arr[]={0,10,40,50,80,100};
    mPlayVolList.clear(); QStringList playVolItems;
    for(double i:play_vol_arr){
        mPlayVolList.push_back(i); playVolItems<<QString::number(i).append('%');
    }
    ui->volume_comboBox->setToolTip("音量");
    ui->volume_comboBox->addItems(playVolItems); //添加音量选项
    ui->volume_comboBox->setCurrentIndex(0); //默认静音
    ui->volume_comboBox->setEditable(false); //设置为不可编辑
    ui->current_video_lineEdit->setReadOnly(true);
    ui->thumbs_dir_lineEdit->setReadOnly(true);
    ui->current_video_lineEdit->setReadOnly(true);
    ui->thumbs_dir_lineEdit->setReadOnly(true);
    exit_after_generate=false;
    // 设置ListView, 绑定点击事件
    StringItemModel=new QStandardItemModel(this);
    connect(ui->thumbs_listView,SIGNAL(clicked(QModelIndex)),this,SLOT(item_clicked(QModelIndex)));
    //update_listview();
    set_video(QStringLiteral()); //初始没有视频,需要在PathLabel, ListView初始化完成后调用
}

//使用FFmpeg播放视频
int ThumbnailsDialog::playVideo(){
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
    //初始化pFormatCtx
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL) != 0){
        qCritical("Initialize pFormatCtx Fail! In %s",__func__);
        return AVFORMAT_OPEN_ERR;
    }
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){ //获取音视频流数据信息
        avformat_close_input(&pFormatCtx);
        qCritical("获取音视频流数据信息 Fail! In %s",__func__);
        return FIND_STREAM_INFO_ERR;
    }
    for(i=0;i<pFormatCtx->nb_streams;i++){ //找到视频流的索引
        if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            streamIndex=i;
            isVideo=0;
            break;
        }
    }
    //没有视频流就退出
    if(isVideo == -1){
        avformat_close_input(&pFormatCtx);
        qCritical("No Video Stream Found! In %s",__func__);
        return NO_STREAMS_ERR;
    }
    //计算 frame_rate 视频帧速率,用于延时
    double frame_rate=av_q2d(pFormatCtx->streams[streamIndex]->r_frame_rate);
    if(std::isnan(frame_rate)){
        frame_rate=av_q2d(pFormatCtx->streams[streamIndex]->avg_frame_rate);
    }
    if(frame_rate==0){ //计算FPS失败，通常情况不会触发这个，可以视情况删除 #2023-04-27
        qCritical("Calc FPS Fail!"); return FIND_STREAM_INFO_ERR;
    }
    int duration_sec=(int)(pFormatCtx->duration/AV_TIME_BASE); //计算视频秒数
    ui->progress_horizontalSlider->setRange(0,duration_sec); //设置QSlider
    ui->progress_horizontalSlider->setValue(0);
    QTime timelab_endtime=QTime(0,0,0).addSecs(duration_sec);
    ui->time_label->setText(QTime(0,0,0).toString()+"/"+timelab_endtime.toString()); //设置时长标签
    mSeekPos=0; mNeedSeek=false; //初始时视频从0开始播放
    pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
    //查找解码器
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec == NULL){
        avcodec_close(pAVctx);
        avformat_close_input(&pFormatCtx);
        qCritical("Find Decoder Fail! In %s",__func__);
        return FIND_DECODER_ERR;
    }
    //初始化pAVctx
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
        avcodec_close(pAVctx);
        avformat_close_input(&pFormatCtx);
        qCritical("Initialize pAVctx Fail! In %s",__func__);
        return AVCODEC_OPEN2_ERR;
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
    //循环读取视频数据
    for(;;){
        if(mPlayState==FINISH_STATE) break; //播放结束
        if(mNeedSeek){
            //偏移到指定位置再开始解码, AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
            av_seek_frame(pFormatCtx, -1, mSeekPos*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
            mNeedSeek=false;
        }
        if(mPlayState==PAUSE_STATE){ //暂停
            delay(300);
        }
        else{ //正在播放
            if(av_read_frame(pFormatCtx,pAVpkt)<0){ //已经没有未解码的数据
                break;
            }
            if(pAVpkt->stream_index==(int)streamIndex){ //如果是视频数据
                //解码一帧视频数据
                /*
                * ## This Method is Deprecated. ##
                * ret=avcodec_decode_video2(pAVctx,pAVframe,&gotPicture,pAVpkt);
                */
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
                    QImage img((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32);
                    ui->video_label->setPixmap(QPixmap::fromImage(img.scaled(scaled_frame_size(img.size(),ui->video_label->size()),Qt::IgnoreAspectRatio,Qt::SmoothTransformation)));
                    long long cur_ms=av_rescale_q(pAVframe->pts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                    if(!ui->progress_horizontalSlider->isSliderDown()){
                        ui->progress_horizontalSlider->setValue(cur_ms/AV_TIME_BASE);
                    }
                    if(mSnapped){
                        mSnappedList.push_back(cur_ms);
                        mSnapped=false;
                        update_listview();
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
    mPlayState=FINISH_STATE; //set finish state
    //qDebug()<<"play finish!";
    return 0;
}



bool ThumbnailsDialog::eventFilter(QObject *watched,QEvent *event){
    if(watched==ui->video_label){
        if(event->type()==QEvent::DragEnter){
            //[[1]]: 鼠标进入label时, label接受拖放的动作, 无论有没有拖拽文件都会触发这个
            QDragEnterEvent *dee=dynamic_cast<QDragEnterEvent *>(event);
            dee->acceptProposedAction();
            return true;
        }
        else if(event->type()==QEvent::Drop){
            //[[2]]: 当放操作发生后, 取得拖放的数据
            QDropEvent *de=dynamic_cast<QDropEvent*>(event);
            QList<QUrl> urls=de->mimeData()->urls();
            if(urls.isEmpty()){return true;}
            QString path=urls.first().toLocalFile();
            //[[3]]: Do Something
            set_video(path);
        }
    }
    return QDialog::eventFilter(watched,event);
}
void ThumbnailsDialog::update_listview(){
    StringItemModel->clear();
    for(auto i:mSnappedList){
        QString str=QTime(0,0,0).addSecs(i/AV_TIME_BASE).toString();
        QStandardItem *item=new QStandardItem(str);
        StringItemModel->appendRow(item);
    }
    ui->thumbs_listView->setModel(StringItemModel); //listview设置Model
}
int ThumbnailsDialog::check_spinbox_valid(){
    int n=ui->row_spinBox->value(),m=ui->column_spinBox->value();
    if(n*m<(int)mSnappedList.size()) return -1;
    return 0;
}

// ListView中的某项被点击，询问是否删除该项
void ThumbnailsDialog::item_clicked(QModelIndex index){
    if(QMessageBox::Yes == QMessageBox::information(this,"提示","是否要删除刚才点击的项目?",QMessageBox::Yes,QMessageBox::No)){
        mSnappedList.remove(index.row());
        update_listview();
    }
}
bool ThumbnailsDialog::set_video(const QString &video_path){ //incompleted
    if(mPlayState!=FINISH_STATE){
        QMessageBox::warning(this,"Fail","当前有未播放完的视频, 稍后再试吧");
        return false;
    }
    if(!video_exists(video_path)&&!video_path.isEmpty()) return false;
    // Need Update Video Info
    mVideoPath=video_path;
    if(mVideoPath.isEmpty()){ //Empty
        ui->current_video_lineEdit->setText("No Video Selected.");
        ui->thumbs_dir_lineEdit->setText("");
    }
    else{
        ui->current_video_lineEdit->setText(mVideoPath);
        ui->thumbs_dir_lineEdit->setText(QFileInfo(mVideoPath).absolutePath());
    }
    mSnappedList.clear(); update_listview(); //Clear Thumbs List
    thumbnails.set_video(mVideoPath);
    return true;
}
void ThumbnailsDialog::on_thumbs_dir_lineEdit_textChanged(const QString &arg1){
    set_thumbs_dir(arg1);
}
bool ThumbnailsDialog::set_thumbs_dir(const QString &t_dir){
    return thumbnails.set_thumbs_dir(t_dir);
}
bool ThumbnailsDialog::set_thumbs_name(const QString &t_name){
    return thumbnails.set_thumbs_name(t_name);
}
void ThumbnailsDialog::set_exit_after_generate(bool flag){
    exit_after_generate=flag;
}
QString ThumbnailsDialog::get_thumbnails_path(){
    return thumbnails.get_thumbnails_path();
}

//窗体变化事件
void ThumbnailsDialog::resizeEvent(QResizeEvent* event){
    Q_UNUSED(event);
    ui->video_label->resize(ui->widget->size());
}
//视频播放/暂停控制
void ThumbnailsDialog::on_playpause_pushButton_clicked(){
    if(!video_exists(mVideoPath)) return;
    if(mPlayState==FINISH_STATE){ //Stop -> Play
        ui->playpause_pushButton->setText("Play");
        mPlayState=PLAY_STATE;
        playVideo();
        on_stop_pushButton_clicked(); //设置标签和停止状态
        return;
    }
    //播放和暂停切换
    if(mPlayState==PLAY_STATE){ //Play -> Pause
        ui->playpause_pushButton->setText("Paused");
        mPlayState=PAUSE_STATE;
    }
    else{ //Pause -> Play
        ui->playpause_pushButton->setText("Play");
        mPlayState=PLAY_STATE;
    }
}
//停止播放视频
void ThumbnailsDialog::on_stop_pushButton_clicked(){
    mPlayState=FINISH_STATE;
    ui->playpause_pushButton->setText("Play");
}
//窗体关闭事件
void ThumbnailsDialog::closeEvent(QCloseEvent* event){
    //没有在播放视频, 直接退出
    if(mPlayState != FINISH_STATE){
        if(QMessageBox::Yes == QMessageBox::information(this,"提示","确认关闭？",QMessageBox::Yes,QMessageBox::No)){
            mPlayState=FINISH_STATE;
            //event->accept();
        }else{
            event->ignore(); //忽略，不关闭
        }
    }
}
//播放速度变化
void ThumbnailsDialog::on_play_spd_comboBox_currentIndexChanged(int index){
    mPlaySpd=mPlaySpdList[index];
}
// Select Video Files
void ThumbnailsDialog::on_select_pushButton_clicked(){
    QString path=QFileDialog::getOpenFileName(this,"选择视频文件","C:\\Users\\Nartsam\\Videos\\Captures","");
    if(!path.isEmpty()){
        mPlayState=FINISH_STATE; //停止正在播放的
        set_video(path);
    }
}
void ThumbnailsDialog::on_change_dir_pushButton_clicked(){
    if(ui->thumbs_dir_lineEdit->text().isEmpty()){
        QMessageBox::warning(this,"无效操作","当前无媒体文件"); return;
    }
    QString dir=QFileDialog::getExistingDirectory(this,"选择目录",ui->thumbs_dir_lineEdit->text(),QFileDialog::ShowDirsOnly);
    if(!dir.isEmpty()) ui->thumbs_dir_lineEdit->setText(dir);
}
void ThumbnailsDialog::on_progress_horizontalSlider_valueChanged(int value){
    QStringList time_list=ui->time_label->text().split('/');
    QString cur_str=QTime(0,0,0).addSecs(value).toString();
    ui->time_label->setText(cur_str+"/"+time_list.back());
}
// 进度条的拖拽和松开
void ThumbnailsDialog::on_progress_horizontalSlider_sliderReleased(){
    int now_pos=ui->progress_horizontalSlider->value();
    mSeekPos=now_pos; mNeedSeek=true;
}
void ThumbnailsDialog::on_snap_pushButton_clicked(){
    if(mPlayState!=PLAY_STATE) return;
    mSnappedList.push_back(0);
    int flag=check_spinbox_valid();
    mSnappedList.pop_back();
    if(flag){
        QMessageBox::warning(this,"错误","手动截取的个数不能多于缩略图的个数");
    }
    else mSnapped=true;
}
void ThumbnailsDialog::on_del_thumb_pushButton_clicked(){} //删除当前选择的ListItem
void ThumbnailsDialog::on_clear_thumbs_pushButton_clicked(){ //清空ListView
    if(QMessageBox::Yes==QMessageBox::information(this,"提示","删除所有手动截取的位置?",QMessageBox::Yes,QMessageBox::No)){
        mSnappedList.clear();
        update_listview();
    }
}
void ThumbnailsDialog::on_column_spinBox_valueChanged(int arg1){
    if(!check_spinbox_valid()) return;
    ui->column_spinBox->setValue(arg1+1);
    QMessageBox::warning(this,"错误","缩略图的个数不能少于手动截取的个数");
}
void ThumbnailsDialog::on_row_spinBox_valueChanged(int arg1){
    if(!check_spinbox_valid()) return;
    ui->row_spinBox->setValue(arg1+1);
    QMessageBox::warning(this,"错误","缩略图的个数不能少于手动截取的个数");
}

void ThumbnailsDialog::on_get_thumbs_pushButton_clicked(){
    int res=thumbnails.get_thumbnails(ui->row_spinBox->value(),ui->column_spinBox->value(),mSnappedList);
    if(exit_after_generate){
        if(res==0){
            QMessageBox::information(this,"Tips","图片生成完成, 即将退出\n路径: "+thumbnails.get_thumbnails_path());
            mPlayState=FINISH_STATE; this->close();
        }
        else QMessageBox::warning(this,"Error","生成图片失败, 详情请参阅日志");
    }
}


#undef PLAY_STATE
#undef FINISH_STATE
#undef PAUSE_STATE
#undef SliderPrecision
