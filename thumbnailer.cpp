#include"thumbnailer.h"
#include"ui_thumbnailer.h"
#include<QPainter>
#include<QCloseEvent>
#include<QPixmap>
#include<QFileDialog>
#include<QMessageBox>
#include<QMimeData>
#include<cstdlib> //for 'system()'
#include"ffmpegplayer.h"
#include"thumbengine.h"
#include"VideoInfo.h"


// 此窗口**没有设置**在关闭时释放资源,需手动调用其析构函数

#define THUMBS_LIMIT 9

long long SliderPrecision=10; //进度条的精度的倒数(秒): e.g.如果为10,则精度为0.1秒
bool Thumbnailer::SlowThumbnailsAlgorithm=false;
bool Thumbnailer::RemoveThumbnailsMark=false;


// ********************* Thumbnailer ************************
Thumbnailer::Thumbnailer(QObject *parent,const QString &path):QObject(parent){
    set_video(path);
}
Thumbnailer::~Thumbnailer(){

}
bool Thumbnailer::set_video(const QString &filename){
    if(!QFileInfo::exists(filename)&&!filename.isEmpty()) return false;
    video_path=filename;
    //update thumbs_dir & thumbs_name
    if(!filename.isEmpty()){
        QFileInfo sinfo(filename);
        thumbs_dir=sinfo.absolutePath()+"/";
        thumbs_name=sinfo.completeBaseName();
    }
    else{thumbs_dir.clear(); thumbs_name.clear();}
    return true;
}
bool Thumbnailer::set_thumbs_dir(const QString &filename){
    if(!QFileInfo::exists(filename)) return false;
    QFileInfo sinfo(filename);
    if(sinfo.isFile()) thumbs_dir=sinfo.absolutePath()+'/';
    else thumbs_dir=sinfo.absoluteFilePath()+'/';
    return true;
}
//该函数没有对输入的合法性进行检查
bool Thumbnailer::set_thumbs_name(const QString &filename){
    thumbs_name=filename; return true;
}

QString Thumbnailer::get_thumbnails_path(){ //没考虑 \ 路径的情况(应该是不用考虑)
    if(thumbs_dir.endsWith('/')) return thumbs_dir+thumbs_name+QStringLiteral(".png");
    return thumbs_dir+QStringLiteral("/")+thumbs_name+QStringLiteral(".png");
}
bool Thumbnailer::get_video_info(const QString &media_path,int &duration,int &width,int &height){
    ThumbEngine tmp_engine;
    VideoInfo res=tmp_engine.get_video_info(media_path);
    if(res.duration<=0) return false;
    duration=res.duration; width=res.width; height=res.height;
    return true;
}

bool Thumbnailer::get_thumbnails(const QString &media_path,int row,int column,const QString &gene_dir,const QString &tbs_name,const QVector<long long> &plist){
    if(row<0||row>THUMBS_LIMIT||column<0||column>THUMBS_LIMIT||plist.size()>row*column){
        qCritical("Argumrnts Invalid! row:%d col:%d plist:%lld.",row,column,plist.size()); return false;
    }
    if(gene_dir.isEmpty()||tbs_name.isEmpty()){qCritical("Generate Dir or ThumbsName is empty."); return false;}
    if(!QFileInfo::exists(gene_dir)){qCritical("Generate Dir is not exist."); return false;}
    //********************************** 合法性检查完成 *************************************//
    QImage result_img;
    try{
        ThumbEngine tmp_engine;
        connect(&tmp_engine,&ThumbEngine::thumbs_progress_changed,this,&Thumbnailer::thumbs_progress_changed);
        result_img=tmp_engine.get_thumbnails(media_path,row,column,plist,SlowThumbnailsAlgorithm,!RemoveThumbnailsMark);
    }
    catch(const char *err){
        QMessageBox::critical(nullptr,"Thumbnailer","Get Thumbnails Fail! Error Info:\n"+QString(err));
        return false;
    }
    if(result_img.isNull()){
        qWarning()<<"Can't save an empty image."; return false;
    }
    QString dist_path(gene_dir);
    if(dist_path.contains('/')&&!dist_path.endsWith('/')) dist_path.append('/');
    if(dist_path.contains('\\')&&!dist_path.endsWith('\\')) dist_path.append('\\');
    dist_path+=(tbs_name+QStringLiteral(".png"));
    if(!result_img.save(dist_path,"PNG")){
        qCritical()<<"Fail to Save Image at:"<<dist_path;
        return false;
    }
    qDebug()<<"Debug: Saved Image at:"<<dist_path;
    return true;
}
bool Thumbnailer::get_thumbnails(int row,int column){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,QVector<long long>());
}
bool Thumbnailer::get_thumbnails(int row,int column,const QVector<long long> &plist){
    return get_thumbnails(video_path,row,column,thumbs_dir,thumbs_name,plist);
}
bool Thumbnailer::get_cover(const QString &media_path,const QString &gene_dir,const QString &cover_name,int snap_sec){
    QVector<long long> v; v.push_back(snap_sec*1000);
    bool lastMarkState=RemoveThumbnailsMark;
    RemoveThumbnailsMark=true;
    int res=get_thumbnails(media_path,1,1,gene_dir,cover_name,v);
    RemoveThumbnailsMark=lastMarkState;
    return res;
}
bool Thumbnailer::get_cover(int snap_sec){
    return get_cover(video_path,thumbs_dir,thumbs_name,snap_sec);
}



// ******************** ThumbnailerDialog *************************
ThumbnailerDialog::ThumbnailerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ThumbnailerDialog){
    ui->setupUi(this);
    ui->video_widget->installEventFilter(this);
    ui->video_widget->setAcceptDrops(true);
    setWindowFlags(Qt::WindowMinimizeButtonHint|Qt::WindowMaximizeButtonHint|Qt::WindowCloseButtonHint);
    //setAttribute(Qt::WA_DeleteOnClose); //关闭窗口时释放资源
    init(); //初始化
    ui->no_watermark_checkBox->setChecked(Thumbnailer::RemoveThumbnailsMark);
    ui->slow_algo_checkBox->setChecked(Thumbnailer::SlowThumbnailsAlgorithm);
    set_video(QStringLiteral()); //初始没有视频. 需要在PathLabel, ListView初始化完成后调用
}
ThumbnailerDialog::~ThumbnailerDialog(){
    delete player;
    delete ui;
}
void ThumbnailerDialog::init(){
    player=new FFmpegPlayer; //不要放置的过于靠后,部分组件初始化时会触发它的成员函数
    player->set_output_widget(ui->video_widget);
    player->set_position_changed_threshold(1000/SliderPrecision);
    snaplist_model=new QStandardItemModel(this); //设置spinBox时会调用它,尽早初始化
    ui->row_spinBox->setRange(1,THUMBS_LIMIT);
    ui->column_spinBox->setRange(1,THUMBS_LIMIT);
    ui->row_spinBox->setValue(3);
    ui->column_spinBox->setValue(3); //初始3*3
    exit_after_generate=false; //默认生成thumbs后不退出
    ui->play_spd_comboBox->setCurrentIndex(3); //设置默认速度1
    ui->play_spd_comboBox->setEditable(false); //设置为不可编辑
    ui->volume_comboBox->setCurrentIndex(0); //默认静音
    ui->volume_comboBox->setEditable(false); //设置为不可编辑
    ui->current_video_lineEdit->setReadOnly(true);
    ui->thumbs_dir_lineEdit->setReadOnly(true);
    ui->current_video_lineEdit->setReadOnly(true);
    ui->thumbs_dir_lineEdit->setReadOnly(true);
    //初始化get_thumbs进度条
    ui->get_thumbs_progressBar->setRange(0,100);
    ui->get_thumbs_progressBar->setValue(0);
    icon_size=QSize(100,50); //设置预览thumbs的图标大小
    //设置ListView, 绑定点击事件
    ui->thumbs_listView->setModel(snaplist_model);
    ui->thumbs_listView->setIconSize(icon_size);
    ui->thumbs_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->thumbs_listView->setSelectionMode(QAbstractItemView::NoSelection);
    //connect(snaplist_model,&QStandardItemModel::dataChanged,this,&::ThumbnailerDialog::update_thumbs_cnt_label_slot); //不管用,只能手动更新
    connect(ui->thumbs_listView,&QListView::clicked,this,&::ThumbnailerDialog::item_clicked);
    connect(&thumbnailer,&Thumbnailer::thumbs_progress_changed,this,&::ThumbnailerDialog::thumbs_progress_changed_slot);
    connect(player,&VideoPlayer::video_changed,this,&::ThumbnailerDialog::video_changed_slot);
    connect(player,&VideoPlayer::duration_changed,this,&::ThumbnailerDialog::duration_changed_slot);
    connect(player,&VideoPlayer::position_changed,this,&::ThumbnailerDialog::position_changed_slot);
    connect(player,&VideoPlayer::state_changed,this,&::ThumbnailerDialog::state_changed_slot);
}
bool ThumbnailerDialog::set_video(const QString &video_path){ //incompleted
    if(player->state()!=VideoPlayer::StopState){
        QMessageBox::warning(this,"Set Video Fail","当前有未播放完的视频, 稍后再试吧");
        return false;
    }
    player->set_video(video_path);
    return true;
}
bool ThumbnailerDialog::set_thumbs_dir(const QString &t_dir){
    return thumbnailer.set_thumbs_dir(t_dir);
}
bool ThumbnailerDialog::set_thumbs_name(const QString &t_name){
    return thumbnailer.set_thumbs_name(t_name);
}
QString ThumbnailerDialog::get_thumbnails_path(){
    return thumbnailer.get_thumbnails_path();
}
void ThumbnailerDialog::set_exit_after_generate(bool flag){
    exit_after_generate=flag;
}

int ThumbnailerDialog::check_spinbox_valid(int more){
    int n=ui->row_spinBox->value(),m=ui->column_spinBox->value();
    if(snaplist_model==nullptr){
        qWarning()<<"snaplist_model is a nullptr, check passed.";
        return 0;
    }
    if(n*m<(int)snaplist_model->rowCount()+more) return -1;
    return 0;
}
void ThumbnailerDialog::update_thumbs_cnt_label(){
    int l=snaplist_model==nullptr?0:snaplist_model->rowCount();
    int r=ui->column_spinBox->value()*ui->row_spinBox->value();
    ui->thumbs_cnt_label->setText(QString::number(l)+QStringLiteral(" / ")+QString::number(r));
}


bool ThumbnailerDialog::eventFilter(QObject *watched,QEvent *event){
    if(watched==ui->video_widget){
        if(event->type()==QEvent::DragEnter){
            //[[1]]: 鼠标进入label时, label接受拖放的动作, 无论有没有拖拽文件都会触发这个
            QDragEnterEvent *dee=dynamic_cast<QDragEnterEvent*>(event);
            dee->acceptProposedAction();
            return true;
        }
        else if(event->type()==QEvent::Drop){
            //[[2]]: 当放操作发生后, 取得拖放的数据
            QDropEvent *de=dynamic_cast<QDropEvent*>(event);
            QList<QUrl> urls=de->mimeData()->urls();
            if(urls.isEmpty()){return true;}
            QString path=urls.first().toLocalFile();
            //[[3]]: Do Something
            set_video(path);
        }
        else if(event->type()==QEvent::MouseButtonDblClick){
            QFileInfo vinfo(ui->current_video_lineEdit->text());
            if(vinfo.exists()){
                QByteArray tmpArr=QString("start /b \"\" \""+vinfo.absoluteFilePath()+"\"").toLocal8Bit();
                system(tmpArr.data());
            }
        }
    }
    return QDialog::eventFilter(watched,event);
}
void ThumbnailerDialog::resizeEvent(QResizeEvent* event){
    Q_UNUSED(event); //暂时应该不需要
    player->refresh_output_widget_size();
}
void ThumbnailerDialog::closeEvent(QCloseEvent* event){
    if(player->state()!=VideoPlayer::StopState){
        if(QMessageBox::Yes==QMessageBox::information(this,"提示","当前视频正在播放, 确认关闭？",QMessageBox::Yes,QMessageBox::No)){
            player->stop(); //event->accept();
        }else{
            event->ignore(); //忽略，不关闭
        }
    } //没有在播放视频, 直接退出
}



/******************* Slot Functions ********************/

void ThumbnailerDialog::video_changed_slot(const QString &video_path){
    if(video_path.isEmpty()){ //Empty
        ui->current_video_lineEdit->setText("No Video Selected.");
        ui->thumbs_dir_lineEdit->setText("");
    }
    else{
        ui->current_video_lineEdit->setText(video_path);
        ui->thumbs_dir_lineEdit->setText(QFileInfo(video_path).absolutePath());
    }
    snaplist_model->clear(); update_thumbs_cnt_label(); //Clear Thumbs List
    thumbnailer.set_video(video_path);
}

void ThumbnailerDialog::state_changed_slot(int state){
    switch(state){
    case VideoPlayer::StopState:{
        ui->playpause_pushButton->setText("Play"); break;
    }
    case VideoPlayer::PlayState:{
        ui->playpause_pushButton->setText("Playing"); break;
    }
    case VideoPlayer::PauseState:{
        ui->playpause_pushButton->setText("Paused"); break;
    }
    default:{
        qWarning("undealed state:%d",state);
    }
    }
}
void ThumbnailerDialog::position_changed_slot(long long pos){
    if(!ui->progress_horizontalSlider->isSliderDown()){
        ui->progress_horizontalSlider->setValue(pos*SliderPrecision/1000);
    }
}
void ThumbnailerDialog::duration_changed_slot(long long duration){
    ui->progress_horizontalSlider->setRange(0,duration*SliderPrecision/1000); //设置QSlider
    ui->progress_horizontalSlider->setValue(0);
    QTime timelab_endtime=QTime(0,0,0).addSecs(duration/1000);
    ui->time_label->setText(QTime(0,0,0).toString()+"/"+timelab_endtime.toString()); //设置时长标签
}
void ThumbnailerDialog::thumbs_progress_changed_slot(double rate){
    int val=(rate*100+0.5);
    val=(val<0)?0:(val>100)?100:val;
    ui->get_thumbs_progressBar->setValue(val);
}

void ThumbnailerDialog::item_clicked(QModelIndex index){
    if(QMessageBox::Yes==QMessageBox::information(this,"提示","是否要删除刚才点击的项目?",QMessageBox::Yes,QMessageBox::No)){
        snaplist_model->removeRow(index.row()); update_thumbs_cnt_label();
    }
}
void ThumbnailerDialog::on_thumbs_dir_lineEdit_textChanged(const QString &arg1){
    set_thumbs_dir(arg1);
}
//视频播放/暂停控制
void ThumbnailerDialog::on_playpause_pushButton_clicked(){
    if(player->video().isEmpty()) return;
    if(player->state()==VideoPlayer::PlayState) player->pause();
    else player->play();
}
//停止播放视频
void ThumbnailerDialog::on_stop_pushButton_clicked(){
    player->stop();
}
void ThumbnailerDialog::on_play_spd_comboBox_currentIndexChanged(int index){
    Q_UNUSED(index);
    player->set_speed(ui->play_spd_comboBox->currentText().toDouble());
}
void ThumbnailerDialog::on_volume_comboBox_currentIndexChanged(int index){
    Q_UNUSED(index);
    player->set_volume(ui->volume_comboBox->currentText().chopped(1).toInt());
}
// Select Video Files
void ThumbnailerDialog::on_select_pushButton_clicked(){
    QString path=QFileDialog::getOpenFileName(this,"选择视频文件","C:\\Users\\Nartsam\\Videos\\Captures","");
    if(!path.isEmpty()){
        player->stop(); //停止正在播放的
        set_video(path);
    }
}
void ThumbnailerDialog::on_change_dir_pushButton_clicked(){
    if(ui->thumbs_dir_lineEdit->text().isEmpty()){
        QMessageBox::warning(this,"无效操作","当前无媒体文件"); return;
    }
    QString dir=QFileDialog::getExistingDirectory(this,"选择目录",ui->thumbs_dir_lineEdit->text(),QFileDialog::ShowDirsOnly);
    if(!dir.isEmpty()) ui->thumbs_dir_lineEdit->setText(dir);
}
void ThumbnailerDialog::on_progress_horizontalSlider_valueChanged(int value){
    QStringList time_list=ui->time_label->text().split('/');
    QString cur_str=QTime(0,0,0).addSecs(value/SliderPrecision).toString();
    ui->time_label->setText(cur_str+"/"+time_list.back());
}
// 进度条的拖拽和松开
void ThumbnailerDialog::on_progress_horizontalSlider_sliderReleased(){
    int now_pos=ui->progress_horizontalSlider->value();
    player->seek(now_pos*1000/SliderPrecision);
}
void ThumbnailerDialog::on_snap_pushButton_clicked(){
    if(player->state()==VideoPlayer::StopState) return;
    if(check_spinbox_valid(1)){
        QMessageBox::warning(this,"错误","手动截取的个数不能多于缩略图的个数");
    }
    else{
        QImage snap_img(player->current_image());
        QPixmap tmp_img=QPixmap::fromImage(snap_img.scaled(icon_size,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        long long pos=player->position();
        snaplist_model->appendRow(new QStandardItem(QIcon(tmp_img),QTime(0,0,0).addMSecs(pos).toString("hh:mm:ss.zzz")));
        update_thumbs_cnt_label();
    }
}
void ThumbnailerDialog::on_clear_thumbs_pushButton_clicked(){ //清空ListView
    if(QMessageBox::Yes==QMessageBox::information(this,"提示","删除所有手动截取的位置?",QMessageBox::Yes,QMessageBox::No)){
        snaplist_model->clear(); update_thumbs_cnt_label();
    }
}
void ThumbnailerDialog::on_column_spinBox_valueChanged(int arg1){
    if(check_spinbox_valid()!=0){
        ui->column_spinBox->setValue(arg1+1);
        QMessageBox::warning(this,"错误","缩略图的个数不能少于手动截取的个数");
    }
    else update_thumbs_cnt_label();
}
void ThumbnailerDialog::on_row_spinBox_valueChanged(int arg1){
    if(check_spinbox_valid()!=0){
        ui->row_spinBox->setValue(arg1+1);
        QMessageBox::warning(this,"错误","缩略图的个数不能少于手动截取的个数");
    }
    else update_thumbs_cnt_label();
}
void ThumbnailerDialog::on_get_thumbs_pushButton_clicked(){
    QVector<long long> snappedList;
    for(int i=0;i<snaplist_model->rowCount();++i){
        QString str=snaplist_model->item(i)->text();
        long long msec=QTime(0,0,0).msecsTo(QTime::fromString(str,"hh:mm:ss.zzz"));
        snappedList.push_back(msec);
    }
    int res=thumbnailer.get_thumbnails(ui->row_spinBox->value(),ui->column_spinBox->value(),snappedList);
    if(exit_after_generate){
        if(res==0){
            QMessageBox::information(this,"Tips","图片生成完成, 即将退出\n保存路径: "+thumbnailer.get_thumbnails_path());
            player->stop(); this->close();
        }
        else QMessageBox::warning(this,"Error","生成图片失败, 详情请参阅日志");
    }
    ui->get_thumbs_progressBar->setValue(0);
}
void ThumbnailerDialog::on_slow_algo_checkBox_clicked(bool checked){Thumbnailer::SlowThumbnailsAlgorithm=checked;}
void ThumbnailerDialog::on_no_watermark_checkBox_clicked(bool checked){Thumbnailer::RemoveThumbnailsMark=checked;}

