#include"thumbengine.h"
#include<QProcess>
#include<QPainter>
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

const int DEFAULT_THUMBS_WIDTH=1024;
const int DEFAULT_THUMBS_HEIGHT=576;

ThumbEngine::ThumbEngine(QObject *parent):QObject(parent){
    ThumbsWidthLimit=DEFAULT_THUMBS_WIDTH;
    ThumbsHeightLimit=DEFAULT_THUMBS_HEIGHT;
}
ThumbEngine::~ThumbEngine(){
}

QImage ThumbEngine::get_thumbnails(const QString &media_path,int row,int column,const QVector<long long> &plist,bool slow_algo,bool watermark){
    if(media_path.isEmpty()){
       throw "Empty media file path."; return QImage();
    }
    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
    int ret,isVideo=0;
    unsigned int i,streamIndex=0;
    AVFormatContext* pFormatCtx=avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){
        throw "Initialize pFormatCtx Fail!"; return QImage();
    }
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        avformat_close_input(&pFormatCtx);
        throw "Get Stream Data Info Fail!"; return QImage();
    }
    for(i=0;i<pFormatCtx->nb_streams;i++){
        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){streamIndex=i; isVideo=1; break;}
    }
    if(!isVideo){
        avformat_close_input(&pFormatCtx); throw "No Video Stream Found!"; return QImage();
    }
    long long duration=pFormatCtx->duration; //计算视频秒数(us)
    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL); //获取视频流编码
    //查找解码器
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){
        avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
        throw "Find Decoder Fail!"; return QImage();
    }
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){
        avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
        throw "Initialize pAVctx Fail!"; return QImage();
    }
    AVPacket *pAVpkt=(AVPacket*)av_malloc(sizeof(AVPacket));
    AVFrame *pAVframe=av_frame_alloc(),*pAVframeRGB=av_frame_alloc();
    unsigned char *buf=(unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1));
    av_image_fill_arrays(pAVframeRGB->data,pAVframeRGB->linesize,buf,AV_PIX_FMT_RGB32,pAVctx->width,pAVctx->height,1);
    struct SwsContext *pSwsCtx=sws_getContext(pAVctx->width,pAVctx->height,pAVctx->pix_fmt,pAVctx->width,pAVctx->height,AV_PIX_FMT_RGB32,SWS_BICUBIC,NULL,NULL,NULL);
    //若row或column有一项是0，根据视频时长决定其大小
    if(!row||!column){
        if(duration/AV_TIME_BASE<300) row=4,column=4; //less than 5 min
        else if(duration/AV_TIME_BASE<3600) row=6,column=6; //less than 1 hour
        else row=8,column=8;
    }
    // *** 计算截取时间戳 ***
    QVector<long long> pts_list; //plist原先存贮的是ms,现在将其转换为符合AV_TIME_BASE的单位
    for(long long i:plist) pts_list.push_back(i*AV_TIME_BASE/1000);
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
    if(pAVctx->width>ThumbsWidthLimit||pAVctx->height>ThumbsHeightLimit){ //超过THUMBS_WIDTH*THUMBS_HEIGHT就缩放
        double fact_w=(double)ThumbsWidthLimit/thumbs_size.width();
        double fact_h=(double)ThumbsHeightLimit/thumbs_size.height();
        if(fact_w<fact_h) thumbs_size=QSize((int)(thumbs_size.width()*fact_w),(int)(thumbs_size.height()*fact_w));
        else thumbs_size=QSize((int)(thumbs_size.width()*fact_h),(int)(thumbs_size.height()*fact_h));
    }
    // 最终的大图
    QImage result(thumbs_size.width()*column,thumbs_size.height()*row,QImage::Format_RGB32);
    QPainter result_painter(&result);
    double progress_rate=0;
    //原始Thumbnailer算法
    for(int n=0;n<row;++n){
        for(int m=0;m<column;++m){
            long long time_stamp=pts_list[n*column+m];
            long long seek_stamp=time_stamp;
            if(slow_algo) seek_stamp=(seek_stamp>9*AV_TIME_BASE)?(seek_stamp-9*AV_TIME_BASE):0;
            av_seek_frame(pFormatCtx,-1,seek_stamp,AVSEEK_FLAG_BACKWARD); //AVSEEK_FLAG_BACKWARD 向后找最近的关键帧
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
                        if(pAVframe->pts==AV_NOPTS_VALUE) current_ms=av_rescale_q(pAVframe->pkt_dts,pFormatCtx->streams[streamIndex]->time_base,{1,AV_TIME_BASE});
                        if(current_ms<time_stamp) continue;
                        sws_scale(pSwsCtx,(const unsigned char* const*)pAVframe->data,pAVframe->linesize,0,pAVctx->height,pAVframeRGB->data,pAVframeRGB->linesize);
                        QImage img=QImage((uchar*)pAVframeRGB->data[0],pAVctx->width,pAVctx->height,QImage::Format_RGB32).scaled(thumbs_size);
                        if(watermark){
                            QPainter img_painter(&img);
                            img_painter.setFont(QFont("Arial",img.height()/15));
                            img_painter.setPen(QPen(Qt::white));
                            img_painter.drawText(10,img.height()/15+10,QTime(0,0,0).addSecs(time_stamp/AV_TIME_BASE).toString());
                        }
                        result_painter.drawImage(m*thumbs_size.width(),n*thumbs_size.height(),img);
                        error_code=0;
                        progress_rate=(double)((n*column+m)+1)/(row*column);
                        emit thumbs_progress_changed(progress_rate);
                        break;
                    }
                }
                av_packet_unref(pAVpkt);
            }
            if(error_code){
                QString errStr=QString("Get %1st Thumbs Fail! ErrCode %2.").arg(QString::number(n*column+m+1),QString::number(error_code));
                throw errStr.toStdString().c_str();
            }
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
    av_frame_free(&pAVframeRGB); av_frame_free(&pAVframe);
    avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
    return result;
}

VideoInfo ThumbEngine::get_video_info(const QString &media_path){
    VideoInfo result;
    std::string convert_str=media_path.toStdString(); const char* videoPath=convert_str.c_str();
    int isVideo=0; unsigned int i,streamIndex=0; AVFormatContext* pFormatCtx=avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx,videoPath,NULL,NULL)!=0){qCritical("Initialize pFormatCtx Fail!"); return result;}
    if(avformat_find_stream_info(pFormatCtx,NULL)<0){qCritical("Get Stream Data Info Fail!"); avformat_close_input(&pFormatCtx); return result;}
    for(i=0;i<pFormatCtx->nb_streams;i++){if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){ streamIndex=i; isVideo=1; break;}}
    if(!isVideo){qCritical("No Video Stream Found!"); avformat_close_input(&pFormatCtx); return result;}
    AVCodecContext *pAVctx=avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pAVctx,pFormatCtx->streams[streamIndex]->codecpar);
    const AVCodec *pCodec=avcodec_find_decoder(pAVctx->codec_id);
    if(pCodec==NULL){avcodec_close(pAVctx); avformat_close_input(&pFormatCtx); qCritical("Find Decoder Fail!"); return result;}
    if(avcodec_open2(pAVctx,pCodec,NULL)<0){avcodec_close(pAVctx); avformat_close_input(&pFormatCtx); qCritical("Initialize pAVctx Fail!"); return result;}
    result.width=pAVctx->width; result.height=pAVctx->height;
    long long duration_us=pFormatCtx->duration; result.duration=duration_us/1000000;
    avcodec_close(pAVctx); avformat_close_input(&pFormatCtx);
    return result;
}


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
