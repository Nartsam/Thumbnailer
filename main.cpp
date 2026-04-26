#include<QApplication>
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
//    td->show_info(level+msg);
}



int main(int argc,char *argv[]){
    QApplication a(argc,argv);

    qInstallMessageHandler(MessageHandler);
    QStringList argList;
    for(int i=1;i<argc;++i) argList.append(QString(argv[i]));

    ThumbnailerDialog* TDialog=new ThumbnailerDialog(nullptr,argList); //需要使用new开启窗口，关闭窗口自动释放资源
    ThumbListener* Listener=new ThumbListener(TDialog); //Dialog关闭时发送信号关闭Listener，不需要手动管理

    //连接Listener的跨线程信号到Dialog,由Qt事件循环自动调度到主线程执行
    QObject::connect(Listener,&ThumbListener::show_info_requested,TDialog,&ThumbnailerDialog::show_info);
    QObject::connect(Listener,&ThumbListener::close_requested,TDialog,&ThumbnailerDialog::close);
    QObject::connect(Listener,&ThumbListener::set_opacity_requested,TDialog,&QWidget::setWindowOpacity);

    Listener->start();
    TDialog->display();

    //td->set_use_snapped_image(true);

    return a.exec();
}

