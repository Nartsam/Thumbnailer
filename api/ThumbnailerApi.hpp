#ifndef THUMBNAILERAPI_HPP
#define THUMBNAILERAPI_HPP

#include<QProcess>
#include<QThread>
#include<QJsonObject>
#include<QJsonDocument>
#include<QEventLoop>
#include<QTimer>
#include<QImage>


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
        if(process.state()!=QProcess::NotRunning) stop_process();
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
     * @thumbs_name: 缩略图文件名(不含扩展名)
     * 缩略图保存位置由Thumbnailer进程管理(ztbso/merged/),通过thumbs_path字段返回
    */
    void start_get_merged_thumbnails(const QString &file_path,int row,int column,const QVector<long long> &pts_list,const QString &thumbs_name){
        QJsonObject obj;
        obj["opt"]="get_merged_thumbnails";
        obj["file_path"]=file_path; obj["row"]=row; obj["column"]=column; obj["thumbs_name"]=thumbs_name;
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
        //断开信号连接,防止超时后回调访问已销毁的局部变量
        QObject::disconnect(getter,&ThumbsGetter::media_info_generated,slot_object,nullptr);
        loop->deleteLater();
        if(width==-1||height==-1){
            getter->deleteLater(); //超时未收到结果,手动释放
            return false;
        }
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
    bool parse_as_thumbnails_result(const QJsonObject &obj){
        if(obj.value("opt")!="get_thumbnails"||!obj.contains("result")) return false; //不是该格式
        int pos=obj.value("pos").toInt();
        QImage image;
        if(obj.value("result").toString()=="Success"){
            QString thumb_path=obj.value("thumb_path").toString();
            if(!thumb_path.isEmpty()) image.load(thumb_path);
            //通知Thumbnailer删除临时文件
            write_json({{"opt","delete_file"},{"file_path",thumb_path}});
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
        QJsonParseError parseError;
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
        QByteArray str;
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
};

}



#endif // THUMBNAILERAPI_HPP
