#include"thumbnailer.h"
#include<QPainter>
#include<QPixmap>
#include<QProcess>
#include<QDir>
#include<queue>
#include<cstring>
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



bool Thumbnailer::DefaultSlowThumbnailsAlgorithm=false;
bool Thumbnailer::DefaultRemoveThumbnailsMark=false;
int Thumbnailer::MaxThumbsLimit=9;
int Thumbnailer::MAX_THUMBS_WIDTH=640; //16*40
int Thumbnailer::MAX_THUMBS_HEIGHT=360; //9*40
int Thumbnailer::ThumbsLimit(){return MaxThumbsLimit;}


// ********************* Thumbnailer ************************
Thumbnailer::Thumbnailer(QObject *parent,const QString &path):QObject(parent){
    set_video(path);
    SlowThumbnailsAlgorithm=DefaultSlowThumbnailsAlgorithm;
    RemoveThumbnailsMark=DefaultRemoveThumbnailsMark;
}
Thumbnailer::~Thumbnailer(){
//    qDebug()<<"Thumbnailer deleted:"<<this;
}
bool Thumbnailer::set_video(const QString &filename){
    if(!QFileInfo::exists(filename)&&!filename.isEmpty()) return false;
    video_path=filename;
    //update thumbs_dir & thumbs_name
    if(!filename.isEmpty()){
        QFileInfo sinfo(filename);
        thumbs_dir=sinfo.absolutePath();
        if(!thumbs_dir.endsWith('/')) thumbs_dir+='/';
        thumbs_name=sinfo.completeBaseName();
    }
    else{thumbs_dir.clear(); thumbs_name.clear();}
    return true;
}
bool Thumbnailer::set_thumbs_dir(const QString &filename){
    if(!QFileInfo::exists(filename)) return false;
    QFileInfo sinfo(filename);
    if(sinfo.isFile()) thumbs_dir=sinfo.absolutePath();
    else thumbs_dir=sinfo.absoluteFilePath();
    if(!thumbs_dir.endsWith('/')) thumbs_dir+='/';
    return true;
}
//该函数没有对输入的合法性进行检查
bool Thumbnailer::set_thumbs_name(const QString &filename){
    thumbs_name=QFileInfo(filename).completeBaseName(); return true;
}
QString Thumbnailer::get_video_path(){
    return video_path;
}
QString Thumbnailer::get_thumbnails_path(char sptor){ //没考虑 \ 路径的情况(应该是不用考虑)
    if(sptor==0){
        if(thumbs_dir.endsWith('/')) return thumbs_dir+thumbs_name+QStringLiteral(".png");
        return thumbs_dir+QStringLiteral("/")+thumbs_name+QStringLiteral(".png");
    }
    if(thumbs_dir.endsWith('/')) return thumbs_dir+QChar(sptor)+thumbs_name+QStringLiteral(".png");
    return thumbs_dir+QStringLiteral("/")+QChar(sptor)+thumbs_name+QStringLiteral(".png");
}

QSize Thumbnailer::calc_size_from_image_list(const QVector<QImage> &v,int maxw,int maxh)const{
    int cnt=0; QSize res(0,0);
    for(const auto &i:v){
        if(i.size().width()<=0||i.size().height()<=0) continue; //排除无效的图片
        if(++cnt==1) res=i.size();
        if(i.size()!=res){
            qWarning("第%d张图像的大小(%d,%d)和第一张图像(%d,%d)不同.",cnt,i.size().width(),i.size().height(),res.width(),res.height());
            if(i.size().width()<res.width()||i.size().height()<res.height()) res=i.size();
        }
    }
    if(res.width()>maxw||res.height()>maxh){
        double fw=(double)maxw/res.width(),fh=(double)maxh/res.height();
        res=(fw<fh)?QSize(int(res.width()*fw),int(res.height()*fw)):QSize(int(res.width()*fh),int(res.height()*fh));
    }
    return res;
}

bool Thumbnailer::get_thumbnails(const QString &media_path,int row,int column,const QString &gene_dir,const QString &tbs_name,const QVector<long long> &plist,const QVector<QImage> &imglist){
    if(gene_dir.isEmpty()||tbs_name.isEmpty()){qCritical("Generate Dir or ThumbsName is empty."); get_thumbnails_result=false; return false;}
    if(!QFileInfo::exists(gene_dir)){qCritical("Generate Dir is not exist."); get_thumbnails_result=false; return false;}
    //********************************** 合法性检查完成 *************************************//
    QVector<QImage> result_image_list; //保存row*col个小图
    bool res=get_thumbnails(media_path,row,column,plist,imglist,&result_image_list);
    if(res){ //执行成功了才有下面的步骤，保证执行成功后result_image_list中有row*col张图片
        QSize thumbs_size=calc_size_from_image_list(result_image_list);
        //最终的大图
        QImage result_img(thumbs_size.width()*column,thumbs_size.height()*row,QImage::Format_RGB32);
        QPainter result_painter(&result_img);
        auto image_it=result_image_list.begin();
        for(int n=0;n<row;++n){
            for(int m=0;m<column;++m){
                QImage d_image(*image_it);
                if(d_image.size()!=QSize(0,0)&&d_image.size()!=thumbs_size) d_image=d_image.scaled(thumbs_size,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                result_painter.drawImage(m*thumbs_size.width(),n*thumbs_size.height(),d_image);
                ++image_it; //(*image_it).save(QString::number(n)+"_"+QString::number(m)+".png");
            }
        }
        if(result_img.isNull()||thumbs_size==QSize(0,0)){
            qWarning()<<(result_img.isNull()?"无法保存空图像.":"未获取到任何有效的图像."); res=false;
        }
        if(res){
            QString dist_path(gene_dir);
            if(dist_path.contains('/')&&!dist_path.endsWith('/')) dist_path.append('/');
            if(dist_path.contains('\\')&&!dist_path.endsWith('\\')) dist_path.append('\\');
            dist_path+=(tbs_name+QStringLiteral(".png"));
            if(!result_img.save(dist_path,"PNG")){
                res=false; qCritical()<<"无法保存图片到该路径:"<<dist_path;
            }
            else{
                qDebug()<<"Debug: Saved Image at:"<<dist_path;
                emit thumbnails_generated(dist_path);
            }
        }
    }
    get_thumbnails_result=res; return res;
}


bool Thumbnailer::get_thumbnails(int row,int column){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,QVector<long long>(),QVector<QImage>());
}
bool Thumbnailer::get_thumbnails(int row,int column,const QVector<long long> &plist,const QVector<QImage> &imglist){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,plist,imglist);
}
bool Thumbnailer::get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec){
    QVector<long long> v; v.push_back(snap_sec*1000);
    bool lastMarkState=RemoveThumbnailsMark;
    RemoveThumbnailsMark=true;
    bool res=get_thumbnails(media_path,1,1,gene_dir,cover_name,v,QVector<QImage>());
    RemoveThumbnailsMark=lastMarkState;
    return res;
}
bool Thumbnailer::get_cover(int snap_sec){
    return get_cover(video_path,thumbs_dir,thumbs_name,snap_sec);
}



VideoInfo Thumbnailer::get_video_info(const QString &media_path){
    VideoInfo result; result.width=result.height=-1;
    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
    int isVideo=0; unsigned int i,streamIndex=0; AVFormatContext* pFormatCtx=avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){qCritical("Initialize pFormatCtx Fail!"); return result;}
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){qCritical("Get Stream Data Info Fail!"); avformat_close_input(&pFormatCtx); return result;}
    for(i=0;i<pFormatCtx->nb_streams;i++){if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){ streamIndex=i; isVideo=1; break;}}
    if(!isVideo){qCritical("No Video Stream Found!"); avformat_close_input(&pFormatCtx); return result;}
    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx); qCritical("Find Decoder Fail!"); return result;}
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx); qCritical("Initialize pAVctx Fail!"); return result;}
    result.width=pAVctx->width; result.height=pAVctx->height;
    long long duration_us=pFormatCtx->duration; result.duration=duration_us/AV_TIME_BASE;
    avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx);
    return result;
}

bool Thumbnailer::get_thumbnails(const QString &media_path,int row,int column,const QVector<long long> &plist,const QVector<QImage> &imglist,QVector<QImage>* reslist){
    int count=row*column;
    if(count<0||count>MaxThumbsLimit*MaxThumbsLimit||plist.size()>count){
        qCritical("Illegal Args! row:%d col:%d plist:%lld.",row,column,plist.size()); get_thumbnails_result=false; return false;
    }
    if(media_path.isEmpty()){
        qCritical()<<"Media Path is Empty."; return false;
    }
    /* ********************************* 参数检查完成 ************************************ */
    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
    int ret,isVideo=0;
    unsigned int i,streamIndex=0;
    AVFormatContext* pFormatCtx=avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){qCritical()<<"初始化 pFormatCtx 失败!"; return false;}
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        avformat_close_input(&pFormatCtx);
        qCritical()<<"获取 Stream Info 失败!"; return false;
    }
    for(i=0;i<pFormatCtx->nb_streams;i++){
        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){streamIndex=i; isVideo=1; break;}
    }
    if(!isVideo){avformat_close_input(&pFormatCtx); qCritical()<<"找不到视频流!"; return false;}
    long long duration=pFormatCtx->duration; //计算视频秒数(us)
    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
    //查找解码器
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx); qCritical()<<"查询解码器失败!"; return false;}
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
        avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx);
        qCritical()<<"初始化 pAVctx 错误!"; return false;
    }
    /* ********************************* 找到了视频流，获取视频时长和尺寸 ************************************ */
    AVPacket *pAVpkt=av_packet_alloc();
    AVFrame *pAVframe=av_frame_alloc(),*pAVframeRGB=av_frame_alloc();
    unsigned char *buf=(unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1));
    av_image_fill_arrays(pAVframeRGB->data,pAVframeRGB->linesize,buf,AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1);
    struct SwsContext *pSwsCtx=sws_getContext(pAVctx->width,pAVctx->height,pAVctx->pix_fmt,pAVctx->width,pAVctx->height,AV_PIX_FMT_RGB32,SWS_BICUBIC,NULL,NULL,NULL);
    //若count是0，根据视频时长决定其大小
    if(count==0){
        if(duration/AV_TIME_BASE<300) row=3,column=3; //少于5min, 3*3
        else if(duration/AV_TIME_BASE<1800) row=4,column=4; //少于30min, 5*5
        else if(duration/AV_TIME_BASE<3600) row=5,column=5; //少于1h, 6*6
        else row=6,column=6;
    }
    // *** 计算截取时间戳 ***
    QVector<long long> pts_list; //plist原先存贮的是ms,现在将其转换为符合AV_TIME_BASE的单位
    for(long long i:plist){
        if(i*AV_TIME_BASE/1000<duration||i>0) pts_list.push_back(i*AV_TIME_BASE/1000); //排除掉不合法的
    }
    std::priority_queue<std::pair<long long,int> > gap_time_queue;
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
    if(pAVctx->width>MAX_THUMBS_WIDTH||pAVctx->height>MAX_THUMBS_HEIGHT){ //超过最大大小就缩放
        double fact_w=(double)MAX_THUMBS_WIDTH/thumbs_size.width();
        double fact_h=(double)MAX_THUMBS_HEIGHT/thumbs_size.height();
        if(fact_w<fact_h) thumbs_size=QSize((int)(thumbs_size.width()*fact_w),(int)(thumbs_size.height()*fact_w));
        else thumbs_size=QSize((int)(thumbs_size.width()*fact_h),(int)(thumbs_size.height()*fact_h));
    }
    /* ********************************* 准备生成图片 ************************************ */
    double progress_rate=0; //当前进度
    if(reslist) reslist->clear();
    //原始Thumbnailer算法
    for(int n=0;n<row;++n){
        for(int m=0;m<column;++m){
            long long time_stamp=pts_list[n*column+m],seek_stamp=time_stamp;
            QImage img; //要获取的小图
            bool got_image=false;
            if(!imglist.isEmpty()){
                long long ms_stamp=time_stamp*1000LL/AV_TIME_BASE; //转换为和plist单位相同的毫秒时间戳
                int p_index=-1;
                for(int i=0;i<(int)plist.size();++i){
                    if(ms_stamp==plist[i]){
                        p_index=i; break;
                    }
                }
                if(p_index>=0&&p_index<(int)imglist.size()){ //找到了时间对应的图像
                    img=imglist[p_index].scaled(thumbs_size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                    got_image=true;
                }
            }
            int error_code=0;
            if(!got_image){
                if(SlowThumbnailsAlgorithm) seek_stamp=(seek_stamp>9*AV_TIME_BASE)?(seek_stamp-9*AV_TIME_BASE):0;
                av_seek_frame(pFormatCtx,-1,seek_stamp,AVSEEK_FLAG_BACKWARD); //AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
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
                            if(pAVframe->pts==AV_NOPTS_VALUE) current_ms=av_rescale_q(pAVframe->pkt_dts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                            if(current_ms<time_stamp) continue;
                            sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
                            img=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32).scaled(thumbs_size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                            error_code=0;
                            break;
                        }
                    }
                    av_packet_unref(pAVpkt);
                }
            }
            if(error_code){
                qCritical()<<QString("Get %1st Thumbs Fail! ErrCode %2.").arg(QString::number(n*column+m+1),QString::number(error_code));
            }
            else{
                if(!RemoveThumbnailsMark){ //添加水印
                    QPainter img_painter(&img);
                    img_painter.setFont(QFont("Arial",img.height()/15));
                    img_painter.setPen(QPen(Qt::white));
                    img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addSecs(time_stamp/AV_TIME_BASE).toString());
                }
                //img.save("test"+QString::number(n*column+m)+".png");
            }
            if(reslist) reslist->push_back(got_image?img:img.copy()); //若img不是.scaled()得到的,就复制一份防止被覆盖
            else qCritical()<<"reslist is nullptr!";
            //更新进度
            progress_rate=(double)((n*column+m)+1)/(row*column);
            emit thumbs_progress_changed(progress_rate);
        }
    }
    /*
     * -- Another Slow Thumbnails Algorithm --
        QVector<long long> msecList(pts_list);
        for(int i=0;i<(int)msecList.size();++i) msecList[i]=((double)msecList[i]/AV_TIME_BASE)*1000;
        result_img=GetThumbnailsByFFmpeg(media_path,row,column,msecList,thumbs_size,!RemoveThumbnailsMark);
    */
    //释放资源
    sws_freeContext(pSwsCtx);
    av_packet_free(&pAVpkt);
    av_frame_free(&pAVframeRGB); av_frame_free(&pAVframe);
    av_free(buf);
    avcodec_free_context(&pAVctx); avformat_close_input(&pFormatCtx);
    return true;
}

//该函数规定了row和col不能超过9，然而新方法中这个函数仅生成图象列表，大图需要自己拼，所以修改了这个函数的实现，旧方法备份如下
//bool Thumbnailer::get_thumbnails(const QString &media_path,int row,int column,const QVector<long long> &plist,const QVector<QImage> &imglist,QVector<QImage>* reslist){
//    if(row<0||row>MaxThumbsLimit||column<0||column>MaxThumbsLimit||plist.size()>row*column){
//        qCritical("参数不合法! row:%d col:%d plist:%lld.",row,column,plist.size()); get_thumbnails_result=false; return false;
//    }
//    if(media_path.isEmpty()){
//        qCritical()<<"视频文件路径为空."; return false;
//    }
//    /* ********************************* 参数检查完成 ************************************ */
//    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
//    int ret,isVideo=0;
//    unsigned int i,streamIndex=0;
//    AVFormatContext* pFormatCtx=avformat_alloc_context();
//    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){qCritical()<<"初始化 pFormatCtx 失败!"; return false;}
//    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
//        avformat_close_input(&pFormatCtx);
//        qCritical()<<"获取 Stream Info 失败!"; return false;
//    }
//    for(i=0;i<pFormatCtx->nb_streams;i++){
//        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){streamIndex=i; isVideo=1; break;}
//    }
//    if(!isVideo){avformat_close_input(&pFormatCtx); qCritical()<<"找不到视频流!"; return false;}
//    long long duration=pFormatCtx->duration; //计算视频秒数(us)
//    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
//    //查找解码器
//    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
//    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
//    if(pCodec==NULL){avcodec_close(pAVctx); avformat_close_input(&pFormatCtx); qCritical()<<"查询解码器失败!"; return false;}
//    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
//        avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
//        qCritical()<<"初始化 pAVctx 错误!"; return false;
//    }
//    /* ********************************* 找到了视频流，获取视频时长和尺寸 ************************************ */
//    AVPacket *pAVpkt=(AVPacket*)av_malloc(sizeof(AVPacket));
//    AVFrame *pAVframe=av_frame_alloc(),*pAVframeRGB=av_frame_alloc();
//    unsigned char *buf=(unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1));
//    av_image_fill_arrays(pAVframeRGB->data,pAVframeRGB->linesize,buf,AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1);
//    struct SwsContext *pSwsCtx=sws_getContext(pAVctx->width,pAVctx->height,pAVctx->pix_fmt,pAVctx->width,pAVctx->height,AV_PIX_FMT_RGB32,SWS_BICUBIC,NULL,NULL,NULL);
//    //若row或column有一项是0，根据视频时长决定其大小
//    if(!row||!column){
//        if(duration/AV_TIME_BASE<300) row=3,column=3; //少于5min, 3*3
//        else if(duration/AV_TIME_BASE<1800) row=5,column=5; //少于30min, 5*5
//        else if(duration/AV_TIME_BASE<3600) row=6,column=6; //少于1h, 6*6
//        else row=7,column=7;
//    }
//    // *** 计算截取时间戳 ***
//    QVector<long long> pts_list; //plist原先存贮的是ms,现在将其转换为符合AV_TIME_BASE的单位
//    for(long long i:plist){
//        if(i*AV_TIME_BASE/1000<duration||i>0) pts_list.push_back(i*AV_TIME_BASE/1000); //排除掉不合法的
//    }
//    std::priority_queue<std::pair<long long,int> > gap_time_queue;
//    long long time_step=(long long)((double)duration/(row*column+1)); //每一段的时间间隔
//    for(long long i=1,pos=time_step;i<=row*column;++i,pos+=time_step){
//        long long gap_time=INT64_MAX;
//        for(auto t:pts_list) gap_time=qMin(gap_time,abs(t-pos));
//        gap_time_queue.push(std::make_pair(gap_time,(int)i));
//    }
//    while(pts_list.size()<column*row){ //选计算的和手动选的差距最大的
//        auto p=gap_time_queue.top(); gap_time_queue.pop();
//        pts_list.push_back(time_step*p.second);
//    }
//    std::sort(pts_list.begin(),pts_list.end());
//    // 计算每张小图的大小
//    QSize thumbs_size(pAVctx->width,pAVctx->height);
//    if(pAVctx->width>MAX_THUMBS_WIDTH||pAVctx->height>MAX_THUMBS_HEIGHT){ //超过最大大小就缩放
//        double fact_w=(double)MAX_THUMBS_WIDTH/thumbs_size.width();
//        double fact_h=(double)MAX_THUMBS_HEIGHT/thumbs_size.height();
//        if(fact_w<fact_h) thumbs_size=QSize((int)(thumbs_size.width()*fact_w),(int)(thumbs_size.height()*fact_w));
//        else thumbs_size=QSize((int)(thumbs_size.width()*fact_h),(int)(thumbs_size.height()*fact_h));
//    }
//    /* ********************************* 准备生成图片 ************************************ */
//    double progress_rate=0; //当前进度
//    if(reslist) reslist->clear();
//    //原始Thumbnailer算法
//    for(int n=0;n<row;++n){
//        for(int m=0;m<column;++m){
//            long long time_stamp=pts_list[n*column+m],seek_stamp=time_stamp;
//            QImage img; //要获取的小图
//            bool got_image=false;
//            if(!imglist.isEmpty()){
//                long long ms_stamp=time_stamp*1000LL/AV_TIME_BASE; //转换为和plist单位相同的毫秒时间戳
//                int p_index=-1;
//                for(int i=0;i<(int)plist.size();++i){
//                    if(ms_stamp==plist[i]){
//                        p_index=i; break;
//                    }
//                }
//                if(p_index>=0&&p_index<(int)imglist.size()){ //找到了时间对应的图像
//                    img=imglist[p_index].scaled(thumbs_size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
//                    got_image=true;
//                }
//            }
//            int error_code=0;
//            if(!got_image){
//                if(SlowThumbnailsAlgorithm) seek_stamp=(seek_stamp>9*AV_TIME_BASE)?(seek_stamp-9*AV_TIME_BASE):0;
//                av_seek_frame(pFormatCtx,-1,seek_stamp,AVSEEK_FLAG_BACKWARD); //AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
//                while(1){
//                    if(av_read_frame(pFormatCtx,pAVpkt)<0){error_code=1; break;}
//                    if(pAVpkt->stream_index==(int)streamIndex){
//                        ret=avcodec_send_packet(pAVctx,pAVpkt);
//                        if(ret<0){
//                            if(error_code==0) error_code=2;
//                            continue;
//                        }
//                        if(avcodec_receive_frame(pAVctx,pAVframe)==0){
//                            long long current_ms=av_rescale_q(pAVframe->pts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
//                            if(pAVframe->pts==AV_NOPTS_VALUE) current_ms=av_rescale_q(pAVframe->pkt_dts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
//                            if(current_ms<time_stamp) continue;
//                            sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
//                            img=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32).scaled(thumbs_size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
//                            error_code=0;
//                            break;
//                        }
//                    }
//                    av_packet_unref(pAVpkt);
//                }
//            }
//            if(error_code){
//                qCritical()<<QString("Get %1st Thumbs Fail! ErrCode %2.").arg(QString::number(n*column+m+1),QString::number(error_code));
//            }
//            else{
//                if(!RemoveThumbnailsMark){ //添加水印
//                    QPainter img_painter(&img);
//                    img_painter.setFont(QFont("Arial",img.height()/15));
//                    img_painter.setPen(QPen(Qt::white));
//                    img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addSecs(time_stamp/AV_TIME_BASE).toString());
//                }
//                //img.save("test"+QString::number(n*column+m)+".png");
//            }
//            if(reslist) reslist->push_back(got_image?img:img.copy()); //若img不是.scaled()得到的,就复制一份防止被覆盖
//            else qCritical()<<"reslist is nullptr!";
//            //更新进度
//            progress_rate=(double)((n*column+m)+1)/(row*column);
//            emit thumbs_progress_changed(progress_rate);
//        }
//    }
//    /*
//     * -- Another Slow Thumbnails Algorithm --
//        QVector<long long> msecList(pts_list);
//        for(int i=0;i<(int)msecList.size();++i) msecList[i]=((double)msecList[i]/AV_TIME_BASE)*1000;
//        result_img=GetThumbnailsByFFmpeg(media_path,row,column,msecList,thumbs_size,!RemoveThumbnailsMark);
//    */
//    //释放资源
//    sws_freeContext(pSwsCtx);
//    av_frame_free(&pAVframeRGB); av_frame_free(&pAVframe);
//    avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
//    return true;
//}




/*
 * 在程序当前目录下创建tmpdir文件夹(若存在则删除并重新创建),调用ffmpeg生成row*col张.png截图,合并之后删除该文件夹
*/
//QImage GetThumbnailsByFFmpeg(const QString &mpath,int row,int column,const QVector<long long> &plist,QSize thumbs_size,bool watermark){
//    if(row*column!=plist.size()){
//        qCritical("Arguments Error, row*col != pts_list size (row:%d,col:%d,plist:%d)",row,column,(int)plist.size());
//        return QImage();
//    }
//    QString FFmpegPath="ffmpeg";
//    QString tmpdir="./temp_thumbs_dir/";
//    if(QDir(tmpdir).exists()) QDir(tmpdir).removeRecursively();
//    QDir().mkdir(tmpdir);
//    int tcnt=0;
//    for(long long i:plist){
//        QString time_str=QTime(0,0,0).addMSecs(i).toString("HH:mm:ss.zzz");
//        QString outfile=tmpdir+QString::number(++tcnt)+".png";
//        //"ffmpeg" -ss 00:03:11.000 -i "test.ts" -vframes 1 -q:v 2 "./fonts/output.png"
//        QString command=QString("\"%1\" -ss %2 -i \"%3\" -vframes 1 -q:v 2 \"%4\"").arg(FFmpegPath,time_str,mpath,outfile);
//        qDebug()<<command;
//        QProcess p; p.start(command); p.waitForFinished();
//    }
//    tcnt=0;
//    QImage result_img(thumbs_size.width()*column,thumbs_size.height()*row,QImage::Format_RGB32);
//    QPainter result_painter(&result_img);
//    for(int n=0;n<row;++n){
//        for(int m=0;m<column;++m){
//            QImage img;
//            if(img.load(tmpdir+QString::number(++tcnt)+".png")){
//                img=img.scaled(thumbs_size);
//                if(watermark){
//                    QPainter img_painter(&img);
//                    img_painter.setFont(QFont("Arial",img.height()/15));
//                    img_painter.setPen(QPen(Qt::white));
//                    img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addMSecs(plist[tcnt-1]).toString());
//                }
//                result_painter.drawImage(m*thumbs_size.width(),n*thumbs_size.height(),img);
//            }
//            else qWarning()<<"Can't load image:"<<(tmpdir+QString::number(++tcnt)+".png");
//        }
//    }
//    QDir(tmpdir).removeRecursively();
//    return result_img;
//}
