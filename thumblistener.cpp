#include"thumblistener.h"
#include"thumbnailer.h"
#include<QFileInfo>
#include<QJsonDocument>
#include<QApplication>
#include<QDir>
#include<algorithm>
#include<windows.h>



ThumbListener::ThumbListener(QObject *parent):QThread{parent}{
    out_stream=std::make_unique<QTextStream>(stdout);
    ensure_data_dirs();
    cleanup_single_dir(); //清理上次运行残留的临时文件(如崩溃后残留)
    init_start_operations();
}
ThumbListener::~ThumbListener(){
    stop();
    if(!this->wait(3000)){ //等待线程退出,最多5秒(CancelIoEx会中断ReadFile阻塞)
        qWarning()<<"Listener线程未能在3秒内退出";
    }
    cleanup_single_dir(); //退出时清理临时文件
    for(QThread *thread:thread_list){
        if(thread!=nullptr&&!thread->isFinished()){
            qDebug()<<"Quit Thread in List:"<<thread;
            thread->quit();
        }
    }
    for(QThread *thread:thread_list) if(thread!=nullptr&&!thread->isFinished()) thread->wait(3000);
    QApplication::quit();
    qDebug()<<"Listener Exit";
}

void ThumbListener::stop(){
    is_stopped=true;
    //中断阻塞的ReadFile调用,使线程能够正常退出
    if(stdin_dup_handle!=nullptr) CancelIoEx(stdin_dup_handle,NULL);
}

void ThumbListener::write_str(const QString &str){
    if(!out_stream) return;
    std::lock_guard<std::mutex> guard(write_lock);
    (*out_stream)<<str<<Qt::endl;
    out_stream->flush();
}
void ThumbListener::write_json(const QJsonObject &obj){
    QJsonDocument jdoc(obj);
    QString str=jdoc.toJson();
    if(use_local8bit) str=QString::fromLocal8Bit(str.toLocal8Bit());
    write_str(str.simplified());
}
void ThumbListener::set_use_local8bit(bool flag){
    use_local8bit=flag;
}

QString ThumbListener::data_root_dir(){
    return QCoreApplication::applicationDirPath()+"/ztbso/";
}
QString ThumbListener::merged_dir(){
    return data_root_dir()+"merged/";
}
QString ThumbListener::single_dir(){
    return data_root_dir()+"single/";
}
void ThumbListener::ensure_data_dirs(){
    QDir dir;
    dir.mkpath(merged_dir());
    dir.mkpath(single_dir());
}
void ThumbListener::cleanup_single_dir(){
    QDir dir(single_dir());
    if(dir.exists()){
        for(const QString &file:dir.entryList(QDir::Files)) dir.remove(file);
    }
}

QString ThumbListener::save_single_thumbnail(const QImage &image,int index){
    ensure_data_dirs();
    QString filename=single_dir()+QString("thumb_%1_%2.png").arg(single_thumb_count++).arg(index);
    if(image.save(filename,"PNG")) return filename;
    qCritical().noquote()<<"Save Single Thumbnails Failed:"<<filename;
    return QString();
}

void ThumbListener::run(){
    bool ok=true;
    char chBuf[4096*10];
    DWORD dwRead;
    HANDLE hStdinDup;
    const HANDLE hStdin=GetStdHandle(STD_INPUT_HANDLE);
    if(hStdin==INVALID_HANDLE_VALUE) return;
    //创建标准输入句柄的副本,关闭原始句柄后仍然能够读取数据
    DuplicateHandle(GetCurrentProcess(),hStdin,GetCurrentProcess(),&hStdinDup,0,false,DUPLICATE_SAME_ACCESS);
    CloseHandle(hStdin);
    stdin_dup_handle=(void*)hStdinDup; //保存句柄,供stop()调用CancelIoEx中断阻塞
    QString line_buffer; //缓存不完整的输入行,避免JSON消息在中间被截断
    while(ok&&!is_stopped){
        ok=ReadFile(hStdinDup,chBuf,sizeof(chBuf),&dwRead,NULL);
        if(ok&&dwRead!=0&&!is_stopped){
            QString input=use_local8bit?QString::fromLocal8Bit(chBuf,dwRead):QString::fromUtf8(chBuf,dwRead);
            line_buffer.append(input);
            //按行分割处理,保留不完整的最后一行到下次读取时拼接
            while(true){
                int idx=line_buffer.indexOf('\n');
                if(idx<0) break;
                QString line=line_buffer.left(idx).trimmed();
                line_buffer=line_buffer.mid(idx+1);
                if(!line.isEmpty()){
//                    emit show_info_requested("Read Input: "+line);
                    parse_json_str(line);
                }
            }
        }
    }
    //处理缓冲区中可能残留的最后一行(没有换行符结尾的情况)
    if(!line_buffer.trimmed().isEmpty()){
        parse_json_str(line_buffer.trimmed());
    }
    //关闭句柄,避免泄漏
    CloseHandle(hStdinDup);
    stdin_dup_handle=nullptr;
}
QThread *ThumbListener::create_thread(QObject *parent){
    QThread *thread=new QThread(parent);
    thread_list.append(thread);
    return thread;
}
void ThumbListener::parse_json_str(const QString &jstr){
    QJsonParseError parseError; QByteArray tmpba=use_local8bit?jstr.toLocal8Bit():jstr.toUtf8();
    QJsonDocument jsonDoc=QJsonDocument::fromJson(tmpba,&parseError);
    if(parseError.error!=QJsonParseError::NoError){
        qCritical().noquote()<<"[ParseJsonStr] Parse Failed:"<<parseError.errorString(); return;
    }
    if(!jsonDoc.isObject()){
        qCritical().noquote()<<"[ParseJsonStr] Result NOT a JSON Object."; return;
    }
    QJsonObject obj=jsonDoc.object();
    QString opt=obj.value("opt").toString().toLower();
    if(start_operations.contains(opt)) start_operations[opt](obj);
    else if(opt=="show"){ //显示窗口
        double opacity=std::clamp(obj["opacity"].toDouble(),0.0,1.0); //控制窗口的opacity属性(越低越不可见)
        if(opacity==0) opacity=1.0; // 0 是无效参数
        emit show_requested(opacity);
        //write_json({{"opt",obj.value("opt").toString()},{"result","Success"}});
    }
    else if(opt=="hide"){ //隐藏窗口(不是最小化)
        emit hide_requested();
        //write_json({{"opt",obj.value("opt").toString()},{"result","Success"}});
    }
    else if(opt=="set_video_path"){
        emit set_video_requested(obj.value("file_path").toString());
    }
    else if(opt=="set_dialog_position"){
        emit set_dialog_position_requested(obj.value("x").toInt(-1),
                                           obj.value("y").toInt(-1),
                                           obj.value("width").toInt(-1),
                                           obj.value("height").toInt(-1));
    }
    else qWarning().noquote()<<QString("[ParseJsonStr] Unknown Command: '%1', RawStr: %2").arg(opt,jstr);
}

void ThumbListener::init_start_operations(){
    if(!start_operations.isEmpty()) return; //只初始化一次
    // >>>>> 指令: get_media_info
    start_operations["get_media_info"]=[this](const QJsonObject &obj){
        QString file_path=obj.value("file_path").toString();
        qint64 task_id=obj.value("task_id").toInteger();
        QString res_str="Success";
        VideoInfo info{0,0,0,0};
        if(!QFileInfo::exists(file_path)){
            qCritical().noquote()<<"[get_media_info] File not Exists:"<<file_path;
            res_str="File not Exists";
        }
        else info=Thumbnailer::get_video_info(file_path);
        QJsonObject res;
        res["opt"]=obj.value("opt").toString();
        res["width"]=info.width; res["height"]=info.height; res["duration"]=info.duration;
        res["task_id"]=task_id;
        res["result"]=res_str;
        write_json(res);
    };
    // >>>>> 指令: get_thumbnails
    start_operations["get_thumbnails"]=[this](const QJsonObject &obj){
        /*
         * @file_path:
         * @count: 一共要多少张
         * @pts_list: 手动截取的视频位置,时间单位为ms,半角逗号分隔
         * 结果以临时文件形式保存在ztbso/single/下,通过thumb_path字段返回文件路径
        */
        QString file_path=obj.value("file_path").toString();
        qint64 task_id=obj.value("task_id").toInteger();
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
        if(res_str!="Success"){
            QJsonObject res;
            res["opt"]=obj.value("opt").toString();
            res["task_id"]=task_id;
            res["result"]=res_str;
            write_json(res);
            return;
        }
        ensure_data_dirs();
        Thumbnailer *thumbnailer=new Thumbnailer();
        //可选字段: 控制生成算法和水印
        if(obj.contains("slow_algorithm")) thumbnailer->SlowThumbnailsAlgorithm=obj.value("slow_algorithm").toBool();
        if(obj.contains("remove_watermark")) thumbnailer->RemoveThumbnailsMark=obj.value("remove_watermark").toBool();
        QThread *thread=create_thread();
        thumbnailer->moveToThread(thread);

        std::shared_ptr<QVector<QImage>> resList(new QVector<QImage>);
        connect(thumbnailer,&Thumbnailer::thumbs_progress_changed,this,[=](double rate){
            QJsonObject progress;
            progress["opt"]=obj.value("opt").toString(); progress["task_id"]=task_id;
            progress["progress"]=(int)(rate*100);
            write_json(progress);
            //所有缩略图生成完毕,保存为临时文件并发送路径
            if((int)(rate*100)>=100){
                for(int i=0;i<resList->size();++i){
                    QJsonObject res;
                    res["opt"]=obj.value("opt").toString(); res["task_id"]=task_id;
                    res["pos"]=i+1;
                    QString thumb_path=save_single_thumbnail(resList->at(i),i);
                    if(!thumb_path.isEmpty()){
                        res["thumb_path"]=thumb_path;
                        res["result"]="Success";
                    }
                    else{
                        res["thumb_path"]="";
                        res["result"]="Save Thumbnails Failed";
                    }
                    write_json(res);
                }
                thumbnailer->deleteLater(); thread->quit();
            }
        });

        thread->start();
        //使用invokeMethod在工作线程上调用,避免共享信号的广播冲突
        QMetaObject::invokeMethod(thumbnailer,[=](){
            bool ok=thumbnailer->get_thumbnails(file_path,1,count,pts_list,QVector<QImage>(),resList.get());
            if(!ok){ //初始化阶段失败,不会触发进度信号
                QJsonObject res;
                res["opt"]=obj.value("opt").toString();
                res["task_id"]=task_id;
                res["result"]="Generate Thumbnails Failed";
                write_json(res);
                thumbnailer->deleteLater(); thread->quit();
            }
        },Qt::QueuedConnection);
    };
    // >>>>> 指令: get_merged_thumbnails
    start_operations["get_merged_thumbnails"]=[this](const QJsonObject &obj){
        /*
         * ---------- Input -----------
         * @file_path:
         * @row, column:
         * @pts_list: 手动截取的视频位置,时间单位为ms,半角逗号分隔
         * @thumbs_name: 同ThumbnailerDialog::set_thumbs_name
         * ---------- Output ----------
         * @opt, task_id, result
         * @thumbs_path: 本地路径(位于ztbso/merged/下)
        */
        QString file_path=obj.value("file_path").toString();
        qint64 task_id=obj.value("task_id").toInteger();
        int row=obj.value("row").toInt(),column=obj.value("column").toInt();
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
        res["opt"]=obj.value("opt").toString(); res["task_id"]=task_id; res["result"]=res_str;
        if(res_str!="Success"){
            write_json(res); return;
        }
        ensure_data_dirs();
        Thumbnailer *thumbnailer=new Thumbnailer();
        //可选字段: 控制生成算法和水印
        if(obj.contains("slow_algorithm")) thumbnailer->SlowThumbnailsAlgorithm=obj.value("slow_algorithm").toBool();
        if(obj.contains("remove_watermark")) thumbnailer->RemoveThumbnailsMark=obj.value("remove_watermark").toBool();
        QThread *thread=create_thread();
        thumbnailer->moveToThread(thread);
        qDebug()<<"created thumbnailer:"<<thumbnailer;
        connect(thumbnailer,&Thumbnailer::thumbs_progress_changed,this,[=](double rate){
            QJsonObject progress;
            progress["opt"]=obj.value("opt").toString(); progress["task_id"]=task_id;
            progress["progress"]=(int)(rate*100);
            write_json(progress);
        });
        connect(thumbnailer,&Thumbnailer::thumbnails_generated,this,[=](const QString &tpath){
            auto tmp_result=res; tmp_result["thumbs_path"]=tpath; //**直接对 res 修改不生效**
            write_json(tmp_result);
            thumbnailer->deleteLater(); thread->quit();
        });
        thumbnailer->set_video(file_path);
        thumbnailer->set_thumbs_dir(merged_dir()); //强制保存到ztbso/merged/
        if(!thumbs_name.isEmpty()) thumbnailer->set_thumbs_name(thumbs_name);
        thread->start();
        QMetaObject::invokeMethod(thumbnailer,[=](){
            bool ok=thumbnailer->get_thumbnails(row,column,pts_list,QVector<QImage>());
            if(!ok){ //初始化阶段失败,thumbnails_generated信号不会触发
                QJsonObject fail_res;
                fail_res["opt"]=obj.value("opt").toString();
                fail_res["task_id"]=task_id;
                fail_res["result"]="Generate Merged Thumbnails Failed";
                write_json(fail_res);
                thumbnailer->deleteLater(); thread->quit();
            }
        },Qt::QueuedConnection);
    };
    // >>>>> 指令: set_window_opacity
    start_operations["set_window_opacity"]=[this](const QJsonObject &obj){
        set_window_opacity(obj);
    };
    // >>>>> 指令: delete_file
    start_operations["delete_file"]=[](const QJsonObject &obj){
        //删除ztbso目录下的临时文件,仅允许删除ztbso目录内的文件
        QString file_path=obj.value("file_path").toString();
        if(file_path.isEmpty()) return;
        QFileInfo fi(file_path);
        if(fi.exists()&&fi.absoluteFilePath().startsWith(QFileInfo(data_root_dir()).absoluteFilePath())){
            QFile::remove(fi.absoluteFilePath());
        }
    };
    // >>>>> 指令: exit
    start_operations["exit"]=[this](const QJsonObject&){
        stop();
        emit close_requested();
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
        emit set_opacity_requested(x); res["result"]="Success";
    }
    write_json(res);
    qDebug().noquote()<<QString("set_window_opacity done: %1").arg(QString::number(x));
}
