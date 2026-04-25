#ifndef THUMBNAILERAPI_HPP
#define THUMBNAILERAPI_HPP

#include<QProcess>
#include<QThread>
#include<QJsonObject>
#include<QJsonDocument>
#include<QSharedMemory>
#include<QEventLoop>
#include<QTimer>
#include<QImage>

//C:\Users\Nartsam\Videos\Captures\TestVideo.mp4
//C:\Users\Nartsam\Videos\Captures\TestVideo - 副本.mp4

//C:\Users\Nartsam\Videos\TestVideo.mp4


namespace ThumbnailerApi{

/*
 * 推荐用法:
 * auto getter=new ThumbsGetter(this);
 * connect(getter,<got_result() 或 某个特定操作的信号>,<处理这个操作的函数>){
 *      // do something
 *      getter->deleteLater(); //完成后销毁
 * }
 * getter->start_xxx
*/


class ThumbsGetter:public QObject{
    Q_OBJECT
public:
    inline static QString ThumbnailerExePath="N:/OI/Code/QtProject/ThumbBuild/build-Thumbnailer-Desktop_Qt_6_5_2_MinGW_64_bit-Release/release/Thumbnailer.exe";

    explicit ThumbsGetter(QObject *parent=nullptr):QObject{parent}{
        connect(&process,&QProcess::readyReadStandardOutput,this,&ThumbsGetter::ready_read_output);
        connect(&process,&QProcess::finished,this,&ThumbsGetter::process_finished);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(ThumbnailerExePath,QStringList{"-nogui"});
        process.waitForStarted();
    }
    ~ThumbsGetter(){
        shared_memory.detach();
        if(process.state()!=QProcess::NotRunning) stop_process();
        // qDebug()<<"ThumbsGetter: Wait For Process Terminate.";
        process.waitForFinished(2000);
        if(process.state()!=QProcess::NotRunning){
            process.terminate();
            qWarning()<<"Try to Terminate Thumbnailer Process";
        }
    }


    //=================================================== 进程启动 ================================================================

    /*
     * 获取一张一张的缩略图, 每生成一张触发一次信号
     * @file_path: 媒体文件路径
     * @count: 总共要生成多少张
    */
    void start_get_thumbnails(const QString &file_path,int count,const QVector<long long> &pts_list){
        QJsonObject obj;
        obj["opt"]="get_thumbnails";
        obj["file_path"]=file_path; obj["count"]=count;
        QString plist_str;
        for(auto i:pts_list) plist_str.append((plist_str.isEmpty()?"":",")+QString::number(i));
        obj["pts_list"]=plist_str;
        write_json(obj);
    }
    /*
     * 获取一整张的缩略图, 生成完成后通过信号给出图片在本地文件中的路径
     * @file_path: 媒体文件路径
     * @row, column: 缩略图的行数和列数
    */
    void start_get_merged_thumbnails(const QString &file_path,int row,int column,const QVector<long long> &pts_list,const QString &thumbs_dir,const QString &thumbs_name){
        QJsonObject obj;
        obj["opt"]="get_merged_thumbnails";
        obj["file_path"]=file_path; obj["row"]=row; obj["column"]=column; obj["thumbs_dir"]=thumbs_dir; obj["thumbs_name"]=thumbs_name;
        QString plist_str;
        for(auto i:pts_list) plist_str.append((plist_str.isEmpty()?"":",")+QString::number(i));
        obj["pts_list"]=plist_str;
        write_json(obj);
    }
    void start_thumbnailer_dialog(double opacity=1.0){
        QJsonObject obj;
        obj["opt"]="start_dialog"; obj["opacity"]=opacity;
        write_json(obj);
    }
    void start_get_media_info(const QString &file_path){
        QJsonObject obj;
        obj["opt"]="get_media_info"; obj["file_path"]=file_path;
        write_json(obj);
    }
    static bool GetMediaInfo(const QString &file_path,QObject *slot_object,int &width,int &height,long long &duration){
        auto getter=new ThumbsGetter();
        QEventLoop *loop=new QEventLoop;
        QTimer timer;
        timer.setSingleShot(true); timer.start(5000); //5秒超时
        QObject::connect(&timer,&QTimer::timeout,loop,&QEventLoop::quit);
        QObject::connect(getter,&ThumbsGetter::media_info_generated,slot_object,[getter,file_path,loop,&width,&height,&duration](QString got_path,int w,int h,long long d){
            if(got_path!=file_path){
                return;
            }
            width=w; height=h; duration=d;
            getter->deleteLater(); //完成后销毁
            loop->quit();
        });
        width=height=-1; //初始化
        getter->start_get_media_info(file_path);
        loop->exec();
        loop->deleteLater();
        if(width==-1||height==-1) return false;
        return true;
    }
private:
    void stop_process(){ //给process(Thumbniler.exe)写入"exit"让它停止
        QJsonObject obj; obj[QStringLiteral("opt")]=QStringLiteral("exit");
        write_json(obj);
    }
    /*
     * 对于某些常用操作,解析完成直接触发对应信号,而不是返回json自己解析
     ***** 注意,解析到操作之后会直接触发对应信号,不需要再次触发
    */
    bool custom_parsed(const QJsonObject &obj){
        if(parse_as_media_info_result(obj)) return true;
        if(parse_as_merged_thumbnails_result(obj)) return true;
        if(parse_as_thumbnails_result(obj)) return true;
        if(is_progress_json(obj)) return true;
        return false;
    }

    bool parse_as_media_info_result(const QJsonObject &obj){
        if(obj.value("opt")!="get_media_info") return false; //不是该格式
        int w=obj.value("width").toInt(),h=obj.value("height").toInt();
        long long duration=obj.value("duration").toInteger();
        if(obj.value("result").toString()!="Success"){
            w=h=-1; duration=-1;
        }
        emit media_info_generated(obj.value("file_path").toString(),w,h,duration);
        return true;
    }
    bool parse_as_merged_thumbnails_result(const QJsonObject &obj){
        if(obj.value("opt")!="get_merged_thumbnails"||!obj.contains("result")) return false; //不是该格式
        QString tpath=obj.value("thumbs_path").toString();
        if(obj.value("result").toString()!="Success"){
            tpath.clear();
        }
        emit local_image_generated(obj.value("file_path").toString(),tpath);
        return true;
    }
    QImage get_image_with_shared_memory_name(const QString &name){
        // 1. 附加到共享内存
        shared_memory.setKey(name);
        if(!shared_memory.attach()){
            qCritical()<<QString("ThumbsGetter: Attach to Thumbs Shared Memory '%1' Failed: %2").arg(name,shared_memory.errorString());
            return QImage();
        }
        // 2. 加锁读取
        shared_memory.lock();
        auto* data=static_cast<const uchar*>(shared_memory.constData());
        // 3. 解析头部
        struct SharedImageHeader {
            qint32 width;           // 图像宽度
            qint32 height;          // 图像高度
            qint32 format;          // QImage::Format 枚举值，如 QImage::Format_ARGB32
            qint32 dataSize;        // 图像像素数据的实际大小（字节数）
            // 注意：这里没有存储像素数据，像素数据紧接着 header 之后存放
        };
        const auto* header=reinterpret_cast<const SharedImageHeader*>(data);
        // 4. 拷贝像素数据！关键：不要直接引用
        const uchar* pixels=data+sizeof(SharedImageHeader);
        QImage shared_image(pixels,header->width,header->height,(QImage::Format)header->format);
        QImage image=shared_image.copy();  // 关键步骤 必须深拷贝！共享内存可能随时被删除！
        // 5. 分离共享内存
        shared_memory.unlock();
        shared_memory.detach();
        return image;
    }
    bool parse_as_thumbnails_result(const QJsonObject &obj){
        if(obj.value("opt")!="get_thumbnails"||!obj.contains("result")) return false; //不是该格式
        int pos=obj.value("pos").toInt();
        QString image_memory_name=obj.value("memory_name").toString();
        QImage image;
        if(obj.value("result").toString()=="Success"){
            image=get_image_with_shared_memory_name(image_memory_name);

            write_json({{"opt","release_memory"},{"memory_name",image_memory_name}}); //向process写入信息释放内存
        }

        emit image_generated(obj.value("file_path").toString(),pos,image);
        return true;
    }
    bool is_progress_json(const QJsonObject &obj){
        if(!obj.contains("progress")||!obj.value("file_path").isString()) return false;
        int x=obj.value("progress").toInt();
        emit thumbs_generating_progress_changed(obj.value("file_path").toString(),x);
        return true;
    }

    void process_line(const QByteArray &str){ //接收到文本输入,一行一行的处理
        if(str_not_a_json(str)){ //不是JSON格式的数据,可能是调试语句,直接打印
            qDebug().noquote()<<"[ThumbnailerPrint]:"<<str;
            return;
        }
        if(print_debug_info) qDebug().noquote()<<"[OriginalRead]:"<<str;
        QJsonParseError parseError; //QByteArray tmpba=transfer_to_local8bit?str.toLocal8Bit():str.toUtf8();
        QJsonDocument jsonDoc=QJsonDocument::fromJson(str,&parseError);
        if(parseError.error!=QJsonParseError::NoError){
            qCritical()<<"ThumbsGetter: Parse Error:"<<parseError.errorString()<<", Input:"<<str.size();
            return;
        }
        if(!jsonDoc.isObject()){qCritical()<<"ThumbsGetter: Result Not Object."; return;}
        QJsonObject obj=jsonDoc.object();
        if(is_progress_json(obj)) return;
        if(obj.value("result").toString().isEmpty()){ //not a result json
            qWarning()<<"ThumbsGetter: Got a Json Object, but Object not a Result Object:"<<str; return;
        }
        if(custom_parsed(obj)) return; //若解析到了对应的操作,信号当时就触发了,这里就不用管了
        if(print_debug_info) qDebug()<<"Will Emit got_result()";
        emit got_result(obj);
    }
    void write_json(const QJsonObject &obj){
        QJsonDocument jdoc(obj);
        QString str=jdoc.toJson();
        if(str.trimmed().isEmpty()) return;
        if(transfer_to_local8bit) str=QString::fromLocal8Bit(str.toLocal8Bit());
        str=str.simplified()+"\n";
        process.write(transfer_to_local8bit?str.toLocal8Bit():str.toUtf8());
        if(print_debug_info) qDebug()<<"write:"<<(transfer_to_local8bit?str.toLocal8Bit():str.toUtf8());
    }
    bool str_not_a_json(const QByteArray &str){ //判断str是不是一个单纯的输出语句,而不是要解析的json
        static const QList<QByteArrayView> InfoPrefix{"Debug: ","Info: ","Warn: ","Error: ","Fatal: "};
        for(const auto &i:InfoPrefix) if(str.startsWith(i)) return true;
        return false;
    }

signals:
    void thumbs_generating_progress_changed(QString file_path,int progress); //progress: 0~100
    void image_generated(QString file_path,int pos,QImage image); //无效的QImage表示操作失败
    void local_image_generated(QString file_path,QString image_path); //image_path为空表示操作失败
    void media_info_generated(QString file_path,int width,int height,long long duration); //w或h < 0 表示操作失败
    /*
     * 该json中必须包含以下两个字段:
     * @opt: 执行的操作
     * @result: 'Success' 表示@opt执行成功,其它字符串表示错误信息
    */
    void got_result(QJsonObject result_json);
private slots:
    void ready_read_output(){
        QByteArray str; //auto str=process.readAllStandardOutput();
        while(true){
            str=process.readLine();
            if(!str.isEmpty()) process_line(str.trimmed()); //原始输入最后还有换行符
            else break;
        }
    }

    void process_finished(int exit_code){  //Thumbnailer进程结束
        qDebug()<<"Thumbnailer Process Finished with:"<<exit_code;
    }
private:
    bool transfer_to_local8bit{false}; //将文本转换为Local8Bit后再写入process,否则以UTF-8格式写入
    bool print_debug_info{false};
    QProcess process;
    QSharedMemory shared_memory;
};

}



#endif // THUMBNAILERAPI_HPP

