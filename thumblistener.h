#ifndef THUMBLISTENER_H
#define THUMBLISTENER_H

#include<QThread>
#include<QJsonObject>
#include<QSharedMemory>
#include"thumbnailerdialog.h" //control dialog


/*
 * 监听外部发来的命令, 控制Thumbnailer完成操作, 发送结果到标准输入输出中
 *
*/



class ThumbListener:public QThread{
    Q_OBJECT
public:
    explicit ThumbListener(QObject *parent=nullptr); //必须以ThumbnailerDialog作为其父亲
    ~ThumbListener();
    void stop();
    void write_str(const QString &str); //最好不要含有换行符
    void write_json(const QJsonObject &obj);
    void set_transfer_to_local8bit(bool flag);

protected:
    void run()override;

private:
    QThread* create_thread(QObject *parent=nullptr);
    void parse_json_str(const QString &jstr);
    void init_start_operations();
    void set_window_opacity(const QJsonObject &obj);

    bool write_image_to_shared_memory(const QImage &image,const QString &memory_name);
    bool release_shared_memory(const QString &memory_name);


signals:
    /*
     * 触发Thumbnailer中将缩略图写入QVector<QImage>中而不保存到本地的函数
     * @参数分别为: media_path,row,col,pList,imgList,resList
    */
    void trigger_thumbnailer_get_thumbnails(const QString&,int,int,const QVector<long long>&,const QVector<QImage>&,QVector<QImage>*);
    /*
     * 触发Thumbnailer中将缩略图写入本地的函数
     * @参数分别为: row,col,pList,imgList
    */
    void trigger_thumbnailer_get_merged_thumbnails(int,int,const QVector<long long>&,const QVector<QImage>&);

private:
    std::mutex write_lock;
    ThumbnailerDialog *tb_dialog{nullptr};
    bool is_stopped{false};
    char command_sptor{':'};
    bool transfer_to_local8bit{true};
    //bool extern_exit{false}; //当程序以独立进程被调用时,启用该选项使listener退出后dialog也退出.非独立进程调用时listener一运行就停止了
    std::unique_ptr<QTextStream> out_stream{nullptr};
    std::unordered_map<QString,std::unique_ptr<QSharedMemory>> shared_memory_list;
    int shared_memory_create_count{0};
    QMap<QString,std::function<void(const QJsonObject &obj)>> start_operations;
    QVector<QThread*> thread_list;
};

#endif // THUMBLISTENER_H
