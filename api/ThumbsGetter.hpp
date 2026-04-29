#ifndef THUMBSGETTER_HPP
#define THUMBSGETTER_HPP


#include<QProcess>
#include<QThread>
#include<QJsonObject>
#include<QJsonDocument>
#include<QEventLoop>
#include<QTimer>
#include<QImage>


namespace ThumbsApi{

/*
 * 推荐用法:
 * auto getter=new ThumbsGetter(this);
 * auto task_id=getter->start_xxx(...);
 * connect(getter,<got_result() 或 某个特定操作的信号>,<处理这个操作的函数>){
 *      if(got_task_id!=task_id) return; //通过task_id匹配结果
 *      // do something
 *      getter->deleteLater(); //完成后销毁
 * }
*/

inline QString ThumbnailerExePath="N:/OI/Code/QtProject/ThumbBuild/build-Thumbnailer-Desktop_Qt_6_5_2_MinGW_64_bit-Release/release/Thumbnailer.exe";

class MediaInfo{
public:
    int width{0};
    int height{0};
    long long duration{0};
    double fps{0};

    MediaInfo()=default;
    MediaInfo(int width,int height,long long duration=0,double fps=0){this->width=width; this->height=height; this->duration=duration; this->fps=fps;}
    void clear(){width=0; height=0; duration=0; fps=0;}
    bool empty()const{
        return duration==0&&width==0&&height==0&&fps==0;
    }
    bool valid()const{ //判断是否存在负数项
        return !(width<0||height<0||duration<0||fps<0);
    }
};

class ThumbsGetter:public QObject{
    Q_OBJECT
public:
    explicit ThumbsGetter(QObject *parent=nullptr);
    ~ThumbsGetter();

signals:
    // 生成缩略图的进度变化了: progress: 0~100
    void thumbs_generating_progress_changed(unsigned long long task_id,int progress);
    //无效的 QImage 表示操作失败
    void image_generated(unsigned long long task_id,int pos,QImage image);
    //image_path 为空表示操作失败
    void local_image_generated(unsigned long long task_id,QString image_path);
    // w<0 或 h<0 表示操作失败
    void media_info_generated(unsigned long long task_id,int width,int height,long long duration);
    /*
     * 该json中必须包含以下两个字段:
     * @opt: 执行的操作
     * @result: 'Success' 表示@opt执行成功,其它字符串表示错误信息
    */
    void got_result(QJsonObject result_json);

public:
    //=================================================== 进程启动 ================================================================

    /*
     * 获取一张一张的缩略图, 每生成一张触发一次信号
     * @file_path: 媒体文件路径
     * @count: 总共要生成多少张
    */
    unsigned long long start_get_thumbnails(const QString &file_path,int count,const QVector<long long> &pts_list);

    /*
     * 获取一整张的缩略图, 生成完成后通过信号给出图片在本地文件中的路径
     * @file_path: 媒体文件路径
     * @row, column: 缩略图的行数和列数
     * @thumbs_name: 缩略图文件名(不含扩展名)
     * 缩略图保存位置由Thumbnailer进程管理(ztbso/merged/),通过thumbs_path字段返回
    */
    unsigned long long start_get_merged_thumbnails(const QString &file_path,int row,int column,const QVector<long long> &pts_list,const QString &thumbs_name);
    unsigned long long start_get_media_info(const QString &file_path);
    void show_thumbnailer_dialog(double opacity=1.0);
    static MediaInfo GetMediaInfo(const QString &file_path); //返回无效的 MediaInfo 表示获取失败

private:
    void stop_process(); //给process(Thumbniler.exe)写入"exit"让它停止

    /*
     * 对于某些常用操作,解析完成直接触发对应信号,而不是返回json自己解析
     ***** 注意,解析到操作之后会直接触发对应信号,不需要再次触发
    */
    bool custom_parsed(const QJsonObject &obj);

    /* 预设好的JSON解析格式, 由 custom_parsed() 检查 */

    // 获取视频信息的结果
    bool parse_as_media_info_result(const QJsonObject &obj);
    bool parse_as_merged_thumbnails_result(const QJsonObject &obj);
    bool parse_as_thumbnails_result(const QJsonObject &obj);
    bool is_progress_json(const QJsonObject &obj);


    void process_line(const QByteArray &str); //接收到文本输入,一行一行的处理
    void write_json(const QJsonObject &obj);
    bool str_not_a_json(const QByteArray &str); //判断str是不是一个单纯的输出语句,而不是要解析的json


private slots:
    void ready_read_output();
    void process_finished(int exit_code);  //Thumbnailer进程结束

private:
    bool use_local8bit{false}; //程序的输入/输出都按照 Local8bit 编解码,否则以UTF-8格式写入
    bool print_debug_info{false};
    unsigned long long next_task_id{1};
    QProcess process;
};


// =================================== 定义部分 ===================================

inline ThumbsGetter::ThumbsGetter(QObject *parent):QObject{parent}{
    connect(&process,&QProcess::readyReadStandardOutput,this,&ThumbsGetter::ready_read_output);
    connect(&process,&QProcess::finished,this,&ThumbsGetter::process_finished);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ThumbnailerExePath,QStringList{"-nogui","-nohotkey"});
    process.waitForStarted();
}

inline ThumbsGetter::~ThumbsGetter(){
    if(process.state()!=QProcess::NotRunning) stop_process();
    process.waitForFinished(2000);
    if(process.state()!=QProcess::NotRunning){
        process.terminate();
        qWarning()<<"Thumbnailer进程未自动退出,尝试强行停止";
    }
}

inline unsigned long long ThumbsGetter::start_get_thumbnails(const QString &file_path,int count,const QVector<long long> &pts_list){
    unsigned long long task_id=next_task_id++;
    QJsonObject obj;
    obj["opt"]="get_thumbnails";
    obj["file_path"]=file_path; obj["count"]=count;
    obj["task_id"]=(qint64)task_id;
    QString plist_str;
    for(auto i:pts_list) plist_str.append((plist_str.isEmpty()?"":",")+QString::number(i));
    obj["pts_list"]=plist_str;
    write_json(obj);
    return task_id;
}

inline unsigned long long ThumbsGetter::start_get_merged_thumbnails(const QString &file_path,int row,int column,const QVector<long long> &pts_list,const QString &thumbs_name){
    unsigned long long task_id=next_task_id++;
    QJsonObject obj;
    obj["opt"]="get_merged_thumbnails";
    obj["file_path"]=file_path; obj["row"]=row; obj["column"]=column; obj["thumbs_name"]=thumbs_name;
    obj["task_id"]=(qint64)task_id;
    QString plist_str;
    for(auto i:pts_list) plist_str.append((plist_str.isEmpty()?"":",")+QString::number(i));
    obj["pts_list"]=plist_str;
    write_json(obj);
    return task_id;
}

inline void ThumbsGetter::show_thumbnailer_dialog(double opacity){
    QJsonObject obj;
    obj["opt"]="start_dialog"; obj["opacity"]=opacity;
    write_json(obj);
}

inline unsigned long long ThumbsGetter::start_get_media_info(const QString &file_path){
    unsigned long long task_id=next_task_id++;
    QJsonObject obj;
    obj["opt"]="get_media_info"; obj["file_path"]=file_path;
    obj["task_id"]=(qint64)task_id;
    write_json(obj);
    return task_id;
}

inline MediaInfo ThumbsGetter::GetMediaInfo(const QString &file_path){
    auto getter=new ThumbsGetter();
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true); timer.start(4000); //4秒超时
    QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);
    unsigned long long task_id=getter->start_get_media_info(file_path);
    bool got_result=false;
    MediaInfo media_info;
    QObject::connect(getter,&ThumbsGetter::media_info_generated,getter,[getter,task_id,&loop,&media_info,&got_result](unsigned long long got_id,int w,int h,long long d){
        if(got_id!=task_id){
            return;
        }
        media_info.width=w; media_info.height=h; media_info.duration=d;
        got_result=true;
        getter->deleteLater(); //完成后销毁
        loop.quit();
    });
    loop.exec();
    //断开信号连接,防止超时后回调访问已销毁的局部变量
    QObject::disconnect(getter,&ThumbsGetter::media_info_generated,getter,nullptr);
//    loop.deleteLater();
    if(!got_result){
        getter->deleteLater(); //超时未收到结果,手动释放
        return MediaInfo{};
    }
    return media_info;
}

inline void ThumbsGetter::stop_process(){ //给process(Thumbniler.exe)写入"exit"让它停止
    QJsonObject obj; obj[QStringLiteral("opt")]=QStringLiteral("exit");
    write_json(obj);
}

inline bool ThumbsGetter::custom_parsed(const QJsonObject &obj){
    if(parse_as_media_info_result(obj)) return true;
    if(parse_as_merged_thumbnails_result(obj)) return true;
    if(parse_as_thumbnails_result(obj)) return true;
    if(is_progress_json(obj)) return true;
    return false;
}

inline bool ThumbsGetter::parse_as_media_info_result(const QJsonObject &obj){
    if(obj.value("opt")!="get_media_info") return false; //不是该格式
    int w=obj.value("width").toInt(),h=obj.value("height").toInt();
    long long duration=obj.value("duration").toInteger();
    if(obj.value("result").toString()!="Success"){
        w=h=-1; duration=-1;
    }
    emit media_info_generated((unsigned long long)obj.value("task_id").toInteger(),w,h,duration);
    return true;
}

inline bool ThumbsGetter::parse_as_merged_thumbnails_result(const QJsonObject &obj){
    if(obj.value("opt")!="get_merged_thumbnails"||!obj.contains("result")) return false; //不是该格式
    QString tpath=obj.value("thumbs_path").toString();
    if(obj.value("result").toString()!="Success"){
        tpath.clear();
    }
    emit local_image_generated((unsigned long long)obj.value("task_id").toInteger(),tpath);
    return true;
}

inline bool ThumbsGetter::parse_as_thumbnails_result(const QJsonObject &obj){
    if(obj.value("opt")!="get_thumbnails"||!obj.contains("result")) return false; //不是该格式
    int pos=obj.value("pos").toInt();
    QImage image;
    if(obj.value("result").toString()=="Success"){
        QString thumb_path=obj.value("thumb_path").toString();
        if(!thumb_path.isEmpty()) image.load(thumb_path);
        //通知Thumbnailer删除临时文件
        write_json({{"opt","delete_file"},{"file_path",thumb_path}});
    }
    emit image_generated((unsigned long long)obj.value("task_id").toInteger(),pos,image);
    return true;
}

inline bool ThumbsGetter::is_progress_json(const QJsonObject &obj){
    if(!obj.contains("progress")||!obj.contains("task_id")) return false;
    int x=obj.value("progress").toInt();
    emit thumbs_generating_progress_changed((unsigned long long)obj.value("task_id").toInteger(),x);
    return true;
}

inline void ThumbsGetter::process_line(const QByteArray &str){ //接收到文本输入,一行一行的处理
    if(str_not_a_json(str)){ //不是JSON格式的数据,可能是调试语句,直接打印
        qDebug().noquote()<<"[TbsGetRaw]:"<<str;
        return;
    }
    if(print_debug_info) qDebug().noquote()<<"[RawStr]:"<<str;
    QJsonParseError parseError;
    QJsonDocument jsonDoc=QJsonDocument::fromJson(str,&parseError);
    if(parseError.error!=QJsonParseError::NoError){
        qCritical()<<"ThumbsGetter: Parse Error:"<<parseError.errorString()<<", Input Size:"<<str.size();
        return;
    }
    if(!jsonDoc.isObject()){qCritical()<<"ThumbsGetter: Result Not Object."; return;}
    QJsonObject obj=jsonDoc.object();
    // 判断是不是预设好解析方式的 JSON
    if(is_progress_json(obj)) return;
    if(obj.value("result").toString().isEmpty()){ //not a result json
        qWarning()<<"ThumbsGetter: Got a Json Object, but Object not a Result Object:"<<str; return;
    }
    if(custom_parsed(obj)) return; //若解析到了对应的操作,信号当时就触发了,这里就不用管了
    // 没有对这个 JSON 预设好解析方式，直接触发 got_result 信号，由接收方自行解析
    if(print_debug_info) qDebug()<<"Will Emit got_result()";
    emit got_result(obj);
}

inline void ThumbsGetter::write_json(const QJsonObject &obj){
    QJsonDocument jdoc(obj);
    QString str=jdoc.toJson();
    if(str.trimmed().isEmpty()) return;
    if(use_local8bit) str=QString::fromLocal8Bit(str.toLocal8Bit());
    str=str.simplified()+"\n";
    process.write(use_local8bit?str.toLocal8Bit():str.toUtf8());
    if(print_debug_info) qDebug()<<"write:"<<(use_local8bit?str.toLocal8Bit():str.toUtf8());
}

inline bool ThumbsGetter::str_not_a_json(const QByteArray &str){ //判断str是不是一个单纯的输出语句,而不是要解析的json
    static const QList<QByteArrayView> InfoPrefix{"Debug: ","Info: ","Warn: ","Error: ","Fatal: "};
    for(const auto &i:InfoPrefix) if(str.startsWith(i)) return true;
    return false;
}

inline void ThumbsGetter::ready_read_output(){
    QByteArray str;
    while(true){
        str=process.readLine();
        if(!str.isEmpty()) process_line(str.trimmed()); //原始输入最后还有换行符
        else break;
    }
}

inline void ThumbsGetter::process_finished(int exit_code){  //Thumbnailer进程结束
    qDebug("Thumbnailer Process Finished with: %d",exit_code);
}

}



#endif // THUMBSGETTER_HPP
