#include"thumblistener.h"
#include<QTextStream>
#include<QFileInfo>
#include<QJsonDocument>
#include<QApplication>
#include<windows.h>



ThumbListener::ThumbListener(QObject *parent):QThread{parent}{
    tb_dialog=static_cast<ThumbnailerDialog*>(parent);
    if(tb_dialog==nullptr){
        qFatal()<<"使用非ThumbnailerDialog指针初始化了Linstener线程.";
    }
    out_stream=std::make_unique<QTextStream>(stdout);
    init_start_operations();
}
ThumbListener::~ThumbListener(){
    stop();
    for(auto &i:shared_memory_list) i.second->detach();  // 清理所有共享内存

    if(!this->isFinished()){
        qWarning()<<"使用stop标志停止Listener线程失败,尝试使用quit()";
        this->quit(); this->wait();
    }
    for(QThread *thread:thread_list){
        if(thread!=nullptr&&!thread->isFinished()){
            qDebug()<<"Quit Thread in List:"<<thread;
            thread->quit();
        }
    }
    for(QThread *thread:thread_list) if(thread!=nullptr&&!thread->isFinished()) thread->wait();
    QApplication::quit();
    qDebug()<<"Listener Exit";
}

void ThumbListener::stop(){is_stopped=true;}

void ThumbListener::write_str(const QString &str){
    if(!out_stream) return;
    write_lock.lock();
    (*out_stream)<<str<<Qt::endl;
    out_stream->flush();
    write_lock.unlock();
}
void ThumbListener::write_json(const QJsonObject &obj){
    QJsonDocument jdoc(obj);
    QString str=jdoc.toJson();
    if(transfer_to_local8bit) str=QString::fromLocal8Bit(str.toLocal8Bit());
    write_str(str.simplified());
}
void ThumbListener::set_transfer_to_local8bit(bool flag){
    transfer_to_local8bit=flag;
}
void ThumbListener::run(){
    bool ok=true; //检查是否成功地从标准输入读取了数据
    char chBuf[4096*10]; //用于存储从标准输入读取的数据
    DWORD dwRead; //用于存储实际读取的字节数
    HANDLE hStdinDup; //定义一个句柄变量hStdinDup，用于存储标准输入的副本的句柄
    const HANDLE hStdin=GetStdHandle(STD_INPUT_HANDLE); //获取标准输入的句柄
    if(hStdin==INVALID_HANDLE_VALUE) return; //检查是否成功获取了标准输入的句柄。如果没有，则直接返回
    //创建一个标准输入句柄的副本，并将其存储在变量hStdinDup中。这样做的目的是为了在关闭原始句柄后仍然能够读取数据
    DuplicateHandle(GetCurrentProcess(),hStdin,GetCurrentProcess(),&hStdinDup,0,false,DUPLICATE_SAME_ACCESS);
    CloseHandle(hStdin); //关闭原始的标准输入句柄。现在只有副本句柄hStdinDup是打开的
    while(ok&&!is_stopped){
        ok=ReadFile(hStdinDup,chBuf,sizeof(chBuf),&dwRead,NULL); //从标准输入的副本中读取数据到chBuf数组中,读取的字节数存储在dwRead中，并更新ok变量的状态
        // emit sig_log(QLatin1String("ok is:")+QString::number(ok));
        if(ok&&dwRead!=0&&!is_stopped){ //检查是否成功读取了数据，并且实际读取的字节数不为0
            QString input=QString::fromUtf8(chBuf,dwRead);
            if(transfer_to_local8bit) input=QString::fromLocal8Bit(chBuf, dwRead);
            tb_dialog->show_info("Read Input: "+input);
            parse_json_str(input);
        }
    }
}
QThread *ThumbListener::create_thread(QObject *parent){
    QThread *thread=new QThread(parent);
    thread_list.append(thread);
    return thread;
}
void ThumbListener::parse_json_str(const QString &jstr){
    QJsonParseError parseError; QByteArray tmpba=transfer_to_local8bit?jstr.toLocal8Bit():jstr.toUtf8();
    QJsonDocument jsonDoc=QJsonDocument::fromJson(tmpba,&parseError);
    if(parseError.error!=QJsonParseError::NoError){
        qCritical()<<"[parse_json_str] Parse Error:"<<parseError.errorString(); return;
    }
    if(!jsonDoc.isObject()){
        qCritical()<<"[parse_json_str] Result Not Object."; return;
    }
    QJsonObject obj=jsonDoc.object();
    QString opt=obj.value("opt").toString().toLower();

    if(start_operations.contains(opt)) start_operations[opt](obj);
    else qWarning()<<QString("[parse_json_str] Unknown Command: '%1', Detail: %2").arg(opt,jstr);
}

void ThumbListener::init_start_operations(){
    if(!start_operations.isEmpty()) return;
    start_operations["get_media_info"]=[this](const QJsonObject &obj){
        QString file_path=obj.value("file_path").toString();
        QString res_str="Success";
        VideoInfo info{0,0,0,0};
        if(!QFileInfo::exists(file_path)){
            qCritical()<<"[get_media_info] File not Exists:"<<file_path;
            res_str="File not Exists";
        }
        else info=Thumbnailer::get_video_info(file_path);
        //QString str=QString("%1,%2,%3").arg(QString::number(info.width),QString::number(info.height),QString::number(info.duration));
        QJsonObject res;
        res["opt"]=obj.value("opt").toString();
        res["width"]=info.width; res["height"]=info.height; res["duration"]=info.duration;
        res["file_path"]=file_path;
        res["result"]=res_str;
        write_json(res);
    };

    start_operations["get_thumbnails"]=[this](const QJsonObject &obj){
        /*
         * @file_path:
         * @count: 一共要多少张
         * @pts_list: 手动截取的视频位置,时间单位为ms,半角逗号分隔
        */
        QString file_path=obj.value("file_path").toString();
        int count=obj.value("count").toInt();
        QStringList pts_strlist=obj.value("pts_list").toString().split(',');
        QVector<long long> pts_list;
        for(const auto &i:pts_strlist){
            if(i.trimmed().isEmpty()) continue;
            bool ok; long long x=i.toLongLong(&ok);
            if(ok) pts_list.push_back(x);
        }
        QString res_str="Success";
        if(!QFileInfo::exists(file_path)) res_str="File not Exists";
        if(count<=0||count>9*9) res_str="Thumbs Count Out of Range";
        if(res_str=="Success"){
            Thumbnailer *thumbnailer=new Thumbnailer(); //tb_dialog获取缩略图只能以保存在本地的形式,使用Thumbnailer获取更细精度的控制
            QThread *thread=create_thread(); //thread->moveToThread(this);
            thumbnailer->moveToThread(thread);
            connect(this,&::ThumbListener::trigger_thumbnailer_get_thumbnails,thumbnailer,
                    static_cast<bool(Thumbnailer::*)(const QString&,int,int,const QVector<long long>&,const QVector<QImage>&,QVector<QImage>*)>(&Thumbnailer::get_thumbnails));

            std::shared_ptr<QVector<QImage>> resList(new QVector<QImage>); //resList保存每一个缩略图
            std::shared_ptr<int> sendCount(new int(0)); //有多少张resList中的图片被发送出去了
            connect(thumbnailer,&Thumbnailer::thumbs_progress_changed,this,[=](double rate){
                QJsonObject progress;
                progress["opt"]=obj.value("opt").toString(); progress["file_path"]=file_path;
                progress["progress"]=(int)(rate*100);
                write_json(progress);
                //查看有没有新图片生成了
                while(resList->size()>*sendCount){
                    QJsonObject res;
                    res["opt"]=obj.value("opt").toString(); res["file_path"]=file_path;
                    res["pos"]=(*sendCount)+1;

                    QString name=QString("SHM%1").arg(QString::number(shared_memory_create_count++));
                    if(write_image_to_shared_memory(resList->at(*sendCount),name)){
                        res["memory_name"]=name;
                        res["result"]="Success";
                    }
                    else{
                        res["memory_name"]="";
                        res["result"]="Write Shared Memory Failed";
                    }

                    ++(*sendCount);
                    write_json(res);
                }
                if(*sendCount>=count){ //图片全部发送完成,销毁资源
                    thumbnailer->deleteLater();
                }
            });
            //
            thread->start();
            emit trigger_thumbnailer_get_thumbnails(file_path,1,count,pts_list,QVector<QImage>(),resList.get());
        }
    };

    start_operations["get_merged_thumbnails"]=[this](const QJsonObject &obj){
        /*
         * ---------- Input -----------
         * @file_path:
         * @row, column:
         * @pts_list: 手动截取的视频位置,时间单位为ms,半角逗号分隔
         * @thumbs_dir: 同ThumbnailerDialog::set_thumbs_dir
         * @thumbs_name: 同ThumbnailerDialog::set_thumbs_name
         * ---------- Output ----------
         * @opt, file_path, result
         * @thumbs_path: 本地路径
        */
        QString file_path=obj.value("file_path").toString();
        int row=obj.value("row").toInt(),column=obj.value("column").toInt();
        QString thumbs_dir=obj.value("thumbs_dir").toString();
        QString thumbs_name=obj.value("thumbs_name").toString();
        QStringList pts_strlist=obj.value("pts_list").toString().split(',');
        QVector<long long> pts_list;
        for(const auto &i:pts_strlist){
            if(i.trimmed().isEmpty()) continue;
            bool ok; long long x=i.toLongLong(&ok);
            if(ok) pts_list.push_back(x);
        }
        QString res_str="Success";
        if(!QFileInfo::exists(file_path)) res_str="File not Exists";
        if(row<=0||row>9||column<=0||column>9) res_str="Row or Column Out of Range";
        QJsonObject res;
        res["opt"]=obj.value("opt").toString(); res["file_path"]=file_path; res["result"]=res_str;
        if(res_str!="Success"){
            write_json(res); return;
        }
        Thumbnailer *thumbnailer=new Thumbnailer(); //使用Thumbnailer获取更细精度的控制
        QThread *thread=create_thread(); //thread->moveToThread(this);
        thumbnailer->moveToThread(thread);
        qDebug()<<"created thumbnailer:"<<thumbnailer;
        connect(this,&::ThumbListener::trigger_thumbnailer_get_merged_thumbnails,thumbnailer,
                static_cast<bool(Thumbnailer::*)(int,int,const QVector<long long>&,const QVector<QImage>&)>(&Thumbnailer::get_thumbnails));
        //connect(thread,&QThread::finished,this,[=](){qDebug()<<"thread finished";});//thread,&QThread::deleteLater);
        connect(thumbnailer,&Thumbnailer::thumbs_progress_changed,this,[=](double rate){
            QJsonObject progress;
            progress["opt"]=obj.value("opt").toString(); progress["file_path"]=file_path;
            progress["progress"]=(int)(rate*100);
            write_json(progress);
        });
        connect(thumbnailer,&Thumbnailer::thumbnails_generated,this,[=](const QString &tpath){ //Done
            auto tmp_result=res; tmp_result["thumbs_path"]=tpath; //**直接对 res 修改不生效**
            write_json(tmp_result);
            thumbnailer->deleteLater(); thread->quit();
        });
        thumbnailer->set_video(file_path);
        thumbnailer->set_thumbs_dir(thumbs_dir); thumbnailer->set_thumbs_name(thumbs_name);
        thread->start();
        emit trigger_thumbnailer_get_merged_thumbnails(row,column,pts_list,QVector<QImage>());
    };

    start_operations["set_window_opacity"]=[](const QJsonObject &){
        qDebug()<<"Undealed Operation: set_window_opacity";
    };

    start_operations["release_memory"]=[this](const QJsonObject &obj){
        QString memory_name=obj.value("memory_value").toString();
        release_shared_memory(memory_name);
    };

    start_operations["exit"]=[this](const QJsonObject&){
        stop();
        tb_dialog->close();
    };
}

/*
 * @opacity: 0~1
*/
void ThumbListener::set_window_opacity(const QJsonObject &obj){
    double x=obj.value("opacity").toDouble(-999);
    QJsonObject res;
    res["opt"]=obj.value("opt").toString();
    if(x<0||x>1){
        qWarning("[set_window_opacity] '%lf'not a valid opacity.",x);
        res["result"]=QString("'%1' not a valid opacity").arg(QString::number(x));
    }
    else{
        tb_dialog->setWindowOpacity(x); res["result"]="Success";
    }
    write_json(obj);
    qDebug()<<QString("set_window_opacity done: %1").arg(QString::number(x));
}




bool ThumbListener::write_image_to_shared_memory(const QImage &image,const QString &memory_name){
    // 1. 先分离现有共享内存
    if(shared_memory_list.find(memory_name)==shared_memory_list.end()) shared_memory_list.insert({memory_name,std::make_unique<QSharedMemory>()});
    std::unique_ptr<QSharedMemory> &shared_memory=shared_memory_list[memory_name];
    if(shared_memory->isAttached()){
        if(!shared_memory->detach()){
            qCritical()<<"在写入共享内存时,分离现有的共享内存失败:"<<shared_memory->error();
            return false;
        }
    }
    // 2. 计算所需大小
    struct SharedImageHeader {
        qint32 width;           // 图像宽度
        qint32 height;          // 图像高度
        qint32 format;          // QImage::Format 枚举值，如 QImage::Format_ARGB32
        qint32 dataSize;        // 图像像素数据的实际大小（字节数）
        // 注意：这里没有存储像素数据，像素数据紧接着 header 之后存放
    };
    int data_size=image.sizeInBytes();
    int total_size=sizeof(SharedImageHeader)+data_size;
    // 3. 设置共享内存
    shared_memory->setKey(memory_name);
    if(!shared_memory->create(total_size)){
        qCritical()<<QString("创建共享内存'%1'失败: %2").arg(memory_name,shared_memory->errorString());
        return false;
    }
    // 4. 加锁访问
    shared_memory->lock();
    auto* data=static_cast<uchar*>(shared_memory->data());
    // 5. 写入头部信息
    auto* header=reinterpret_cast<SharedImageHeader*>(data);
    header->width=image.width();
    header->height=image.height();
    header->format=image.format();
    header->dataSize=data_size;
    // 6. 写入像素数据
    uchar* pixels=data+sizeof(SharedImageHeader);
    memcpy(pixels,image.constBits(),data_size);
    shared_memory->unlock();

    return true;
}
bool ThumbListener::release_shared_memory(const QString &memory_name){
    if(shared_memory_list.find(memory_name)==shared_memory_list.end()) return true;
    auto &shm=shared_memory_list[memory_name];
    if(!shm->detach()){
        qCritical().noquote()<<QString("释放共享内存 '%1' 失败: %2").arg(memory_name,shm->errorString());
        return false;
    }
    shm.release();
    shared_memory_list.erase(memory_name);
    return true;
}
