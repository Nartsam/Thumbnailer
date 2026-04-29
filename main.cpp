#include<QApplication>
#include<QJsonObject>
#include<iostream>
#include"thumbnailerdialog.h"
#include"thumblistener.h"



void MessageHandler(QtMsgType type,const QMessageLogContext &context,const QString &msg){
    Q_UNUSED(context);
    static std::mutex MessageHandlerMutex;
    QString level;
    if(type==QtDebugMsg) level=QStringLiteral("Debug: ");
    else if(type==QtWarningMsg) level=QStringLiteral("Warn:  ");
    else if(type==QtCriticalMsg) level=QStringLiteral("Error: ");
    else if(type==QtFatalMsg) level=QStringLiteral("Fatal: ");
    else if(type==QtInfoMsg) level=QStringLiteral("Info:  ");

    std::lock_guard<std::mutex> message_locker(MessageHandlerMutex);
    QByteArray outstr=QString(level+msg).toLocal8Bit();
    std::cout<<outstr.data()<<std::endl;
}



int main(int argc,char *argv[]){
    QApplication a(argc,argv);

    qInstallMessageHandler(MessageHandler);
    QStringList argList;
    for(int i=1;i<argc;++i) argList.append(QString(argv[i]));

    ThumbnailerDialog* TDialog=new ThumbnailerDialog(nullptr,argList); //需要使用new开启窗口，关闭窗口自动释放资源
    ThumbListener* Listener=new ThumbListener(TDialog); //Dialog关闭时发送信号关闭Listener，不需要手动管理

    if(argList.contains("-nogui",Qt::CaseInsensitive)){ //在 -nogui 模式下固定 dialog 生成目录，并把 thumbnails_generated 转成 JSON 回传
        TDialog->set_fixed_thumbs_dir(ThumbListener::merged_dir());
    }

    //连接Listener的跨线程信号到Dialog,由Qt事件循环自动调度到主线程执行
    QObject::connect(Listener,&ThumbListener::show_info_requested,TDialog,&ThumbnailerDialog::show_info);
    QObject::connect(Listener,&ThumbListener::close_requested,TDialog,&ThumbnailerDialog::close);
    QObject::connect(Listener,&ThumbListener::set_opacity_requested,TDialog,&QWidget::setWindowOpacity);
    QObject::connect(Listener,&ThumbListener::show_requested,TDialog,[TDialog](double opacity){
        TDialog->setWindowOpacity(opacity);
        TDialog->show();
        TDialog->raise();
        TDialog->activateWindow();
    });
    QObject::connect(Listener,&ThumbListener::hide_requested,TDialog,&QWidget::hide);
    QObject::connect(Listener,&ThumbListener::set_video_requested,TDialog,[Listener,TDialog](const QString &video_path){
        QJsonObject res;
        res["opt"]="set_video_path";
        res["file_path"]=video_path;
        res["result"]=TDialog->set_video(video_path)?"Success":"Set Video Failed";
        Listener->write_json(res);
    });
    QObject::connect(Listener,&ThumbListener::set_dialog_position_requested,TDialog,[Listener,TDialog](int x,int y,int width,int height){
        QJsonObject res;
        res["opt"]="set_dialog_position";
        res["x"]=x; res["y"]=y;
        res["width"]=width; res["height"]=height;
        if(width<-1||height<-1||width==0||height==0||x<-1||y<-1){
            res["result"]="Invalid Dialog Geometry";
        }
        else{
            TDialog->setGeometry(x==-1?TDialog->x():x,y==-1?TDialog->y():y,width==-1?TDialog->width():width,height==-1?TDialog->height():height);
            res["result"]="Success";
        }
        Listener->write_json(res);
    });
    QObject::connect(TDialog,&ThumbnailerDialog::thumbnails_generated,Listener,[Listener](const QString &image_path){
        QJsonObject res;
        res["opt"]="dialog_image_generated";
        res["image_path"]=image_path;
        res["result"]=image_path.isEmpty()?"Image Path Is Empty":"Success";
        Listener->write_json(res);
    });


    //td->set_use_snapped_image(true);


    Listener->start();
    TDialog->display();

    return a.exec();
}
