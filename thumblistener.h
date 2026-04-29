#ifndef THUMBLISTENER_H
#define THUMBLISTENER_H

#include<QThread>
#include<QJsonObject>
#include<QTextStream>
#include<QImage>
#include<QMap>
#include<QVector>
#include<atomic>
#include<mutex>
#include<functional>


/*
 * 监听外部发来的命令, 控制Thumbnailer完成操作, 发送结果到标准输入输出中
 * 通过信号与主线程的GUI对象通信,避免跨线程直接操作UI
 * 所有运行时数据存放在可执行文件同级目录的ztbso文件夹中:
 *   ztbso/merged/ - 整张缩略图
 *   ztbso/single/ - 单张小缩略图(临时文件,程序退出时清理)
*/


class ThumbListener:public QThread{
    Q_OBJECT
public:
    explicit ThumbListener(QObject *parent=nullptr);
    ~ThumbListener();
    void stop();
    void write_str(const QString &str); //最好不要含有换行符
    void write_json(const QJsonObject &obj);
    void set_use_local8bit(bool flag);

    // 数据目录管理
    static QString data_root_dir();
    static QString merged_dir();
    static QString single_dir();
    static void ensure_data_dirs(); //确保 merged_dir() 和 single_dir() 存在
    static void cleanup_single_dir();

signals:
    //跨线程信号: 通知主线程执行GUI操作,由Qt事件循环调度到主线程
    void show_info_requested(const QString &info);
    void close_requested();
    void set_opacity_requested(double opacity);
    void show_requested(double opacity);
    void hide_requested();
    void set_video_requested(const QString &video_path);
    void set_dialog_position_requested(int x,int y,int width,int height);

protected:
    void run()override;

private:
    QThread* create_thread(QObject *parent=nullptr);
    void parse_json_str(const QString &jstr);
    void init_start_operations(); //创建 start 类行为的 lambda 表达式
    void set_window_opacity(const QJsonObject &obj);
    QString save_single_thumbnail(const QImage &image,int index);

private:
    std::mutex write_lock;
    std::atomic<bool> is_stopped{false};
    bool use_local8bit{true}; //程序的输入/输出都按照 Local8bit 编解码
    std::unique_ptr<QTextStream> out_stream{nullptr};
    int single_thumb_count{0};
    QMap<QString,std::function<void(const QJsonObject &obj)>> start_operations; //<指令名-对应的lambda函数>
    QVector<QThread*> thread_list;
    void *stdin_dup_handle{nullptr}; //ReadFile用的句柄副本,用于CancelIoEx中断阻塞读取
};

#endif // THUMBLISTENER_H
