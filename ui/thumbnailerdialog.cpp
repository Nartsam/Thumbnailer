#include"thumbnailerdialog.h"
#include"ui_thumbnailerdialog.h"
#include<QTimer>
#include<QThread>
#include<QMimeData>
#include<QFileDialog>
#include<QMessageBox>
#include<QCloseEvent>
#include"videoplayer.h"
#include"ffmpegplayer.h"
#include"potplayer.h"
#include"gifencoder.h" //For Generate GIF
#include<windows.h>      //RegisterHotKey, GetAsyncKeyState等
#include<QKeySequence>   //用于将Qt按键组合转为可读字符串(如"Ctrl+S")




// 此窗口**设置**在关闭时释放资源,请使用new获取

#define STOP_CHAR   QStringLiteral("⏹")
#define PLAY_CHAR   QStringLiteral("►")
#define PAUSE_CHAR  QStringLiteral("⏸")
#define FFWD_CHAR   QStringLiteral("⏩")
#define FBWD_CHAR   QStringLiteral("⏪")
#define SNAP_CHAR   QStringLiteral("Snap")
#define SELECT_CHAR QStringLiteral("⏏")
#define CHGDIR_CHAR QStringLiteral("🗁")


static const long long SliderPrecision=10; //进度条的精度的倒数(秒): e.g.如果为10,则精度为0.1秒
//将Qt修饰键转换为Win32 MOD_*常量, 供RegisterHotKey使用
static UINT qtModifiersToNative(Qt::KeyboardModifiers mod){
    UINT n=0;
    if(mod&Qt::ControlModifier) n|=MOD_CONTROL;
    if(mod&Qt::AltModifier)     n|=MOD_ALT;
    if(mod&Qt::ShiftModifier)   n|=MOD_SHIFT;
    if(mod&Qt::MetaModifier)    n|=MOD_WIN;
    return n;
}
//将Qt::Key转换为Win32虚拟键码(VK_*), Qt::Key_A~Z和0~9的值恰好与VK码相同
static UINT qtKeyToNativeVk(Qt::Key key){
    if(key>=Qt::Key_A&&key<=Qt::Key_Z) return key;
    if(key>=Qt::Key_0&&key<=Qt::Key_9) return key;
    if(key>=Qt::Key_F1&&key<=Qt::Key_F24) return VK_F1+(key-Qt::Key_F1);
    switch(key){
    case Qt::Key_Space:  return VK_SPACE;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Tab:    return VK_TAB;
    default: return 0;
    }
}
ThumbnailerDialog::PlayerList ThumbnailerDialog::default_video_player=ThumbnailerDialog::Player_PotPlayer;



void ThumbnailerDialog::SetDefaultVideoPlayer(PlayerList defPlayer){
    default_video_player=defPlayer;
}

ThumbnailerDialog::ThumbnailerDialog(QWidget *parent,const QStringList &args):QDialog(parent),ui(new Ui::ThumbnailerDialog){
    player=nullptr; //必须要置空,否则可能访问非法地址
    ui->setupUi(this);
    ui->video_widget->installEventFilter(this);
    ui->video_widget->setAcceptDrops(true);
    setWindowFlags(Qt::WindowMinimizeButtonHint|
                   Qt::WindowMaximizeButtonHint|
                   Qt::WindowCloseButtonHint|
                   Qt::Window); //确保Thumbnailer是一个独立的窗口
    setAttribute(Qt::WA_DeleteOnClose); //关闭窗口时释放资源
    //this->grabKeyboard(); //捕捉键盘事件
    set_arguments(args);
    init(); //初始化
    ui->no_watermark_checkBox->setChecked(Thumbnailer::DefaultRemoveThumbnailsMark);
    ui->slow_algo_checkBox->setChecked(Thumbnailer::DefaultSlowThumbnailsAlgorithm);
}
ThumbnailerDialog::~ThumbnailerDialog(){
    //注销所有已注册的全局热键
    for(const auto &e:registeredHotkeys) UnregisterHotKey((HWND)winId(),e.id);
    registeredHotkeys.clear();
    hotkeyIdToName.clear();
    delete player;
    delete ui;
    qDebug()<<"ThumbnailerDialog Exit.";
}

void ThumbnailerDialog::set_arguments(const QStringList &arg_list){
    if(arg_list.contains("-nogui",Qt::CaseInsensitive)) no_gui=true;
}

void ThumbnailerDialog::show_info(const QString &str){
    ui->current_video_lineEdit->setText(str);
}
void ThumbnailerDialog::init(){
    //player的初始化不要放置的过于靠后,部分组件初始化时会触发它的成员函数
    set_video_player_by_name(default_video_player);
    setup_settings();
    is_recording_gif=false;
    gif_grab_timer=nullptr;
    snaplist_model=new QStandardItemModel(this); //设置spinBox时会调用它,尽早初始化
    snapped_image_list.clear();
    ui->row_spinBox->setRange(1,Thumbnailer::ThumbsLimit());
    ui->column_spinBox->setRange(1,Thumbnailer::ThumbsLimit());
    ui->row_spinBox->setValue(3); ui->column_spinBox->setValue(3); //初始3*3
    ui->play_spd_comboBox->setCurrentIndex(3); //设置默认速度1
    ui->play_spd_comboBox->setEditable(false); //设置为不可编辑
    ui->volume_comboBox->setCurrentIndex(5); //默认100%音量
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

    set_video(QStringLiteral()); //初始没有视频. 需要在PathLabel, ListView初始化完成后调用
    set_buttons_icon();
    // 注册默认的快捷键
    set_global_hotkey("snap",Qt::ControlModifier,Qt::Key_S); //ctrl+s 捕获
}

void ThumbnailerDialog::setup_settings(){
    hide_window_when_generating=false;
    exit_after_generate=false; //默认生成thumbs后不退出
    use_snapped_image=false; //默认生成缩略图时按照时间重新生成
    print_image_save_path=true;
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
void ThumbnailerDialog::display(){
    if(!no_gui) this->show();
}
void ThumbnailerDialog::set_video_file_widgets_enable(bool flag){
    ui->current_video_lineEdit->setEnabled(flag);
    ui->thumbs_dir_lineEdit->setEnabled(flag);
    ui->change_dir_pushButton->setEnabled(flag);
}
QString ThumbnailerDialog::get_thumbnails_path(){
    return thumbnailer.get_thumbnails_path();
}
QString ThumbnailerDialog::get_video_player_name(){
    return player==nullptr?nullptr:player->video_player_name();
}
QWidget *ThumbnailerDialog::get_video_output_widget()const{
    return ui->video_widget;
}
void ThumbnailerDialog::set_exit_after_generate(bool flag){exit_after_generate=flag;}
void ThumbnailerDialog::set_hide_window_when_generating(bool flag){hide_window_when_generating=flag;}
void ThumbnailerDialog::set_print_image_save_path(bool flag){print_image_save_path=flag;}
void ThumbnailerDialog::set_use_snapped_image(bool flag){
    bool lastst=use_snapped_image;
    use_snapped_image=flag;
    if(!use_snapped_image) snapped_image_list.clear();
    if(!lastst&&flag && snaplist_model!=nullptr&&snaplist_model->rowCount()>0){ //从无到有 且 列表中已有截图
        qWarning()<<"开启 use_snapped_image 项时已有截图, 清空当前截图后再开启吧.";
        use_snapped_image=lastst;
    }
}
void ThumbnailerDialog::set_video_player_by_name(PlayerList player_name){
    if(player_name==PlayerList::Player_PotPlayer) set_video_player(new PotPlayer);
    //else if(player_name==PlayerList::Player_MPlayer) set_video_player(new MPlayer);
    else set_video_player(new FFmpegPlayer);
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
void ThumbnailerDialog::set_ui_state(bool flag){
    ui->playpause_pushButton->setEnabled(flag); ui->select_pushButton->setEnabled(flag); ui->change_dir_pushButton->setEnabled(flag);
    ui->clear_thumbs_pushButton->setEnabled(flag); ui->get_thumbs_pushButton->setEnabled(flag); ui->snap_pushButton->setEnabled(flag);
    ui->stop_pushButton->setEnabled(flag); ui->progress_horizontalSlider->setEnabled(flag); ui->slow_algo_checkBox->setEnabled(flag);
    ui->no_watermark_checkBox->setEnabled(flag); ui->play_spd_comboBox->setEnabled(flag); ui->volume_comboBox->setEnabled(flag);
    ui->thumbs_listView->setEnabled(flag); ui->thumbs_dir_lineEdit->setEnabled(flag); ui->current_video_lineEdit->setEnabled(flag);
    ui->column_spinBox->setEnabled(flag); ui->row_spinBox->setEnabled(flag);
}

void ThumbnailerDialog::set_video_player(VideoPlayer *new_player){
    //注意不要使用 this->player 该函数会在构造函数中调用,此时 this 还没有初始化完成
    if(player!=nullptr&&player->state()!=VideoPlayer::PlayerState::StopState){
        qCritical()<<"can't change video player when playing!";
        delete new_player;
        return;
    }
    int last_pos_changed_threshold=1000/SliderPrecision;
    QWidget *last_output_widget=ui->video_widget;
    if(player!=nullptr){ //获取原播放器的一些设置
        last_pos_changed_threshold=player->position_changed_threshold();
        delete player;
    }
    player=new_player;
    //上个播放器的设置并没有恢复,只保留了pos_threshold
    player->set_output_widget(last_output_widget);
    player->set_position_changed_threshold(last_pos_changed_threshold);
    //连接信号
    connect(player,&VideoPlayer::video_changed,this,&::ThumbnailerDialog::video_changed_slot);
    connect(player,&VideoPlayer::duration_changed,this,&::ThumbnailerDialog::duration_changed_slot);
    connect(player,&VideoPlayer::position_changed,this,&::ThumbnailerDialog::position_changed_slot);
    connect(player,&VideoPlayer::state_changed,this,&::ThumbnailerDialog::state_changed_slot);
}

void ThumbnailerDialog::set_buttons_icon(){
    ui->select_pushButton->setText(SELECT_CHAR); ui->change_dir_pushButton->setText(CHGDIR_CHAR);
    ui->snap_pushButton->setText(SNAP_CHAR); ui->stop_pushButton->setText(STOP_CHAR);
    ui->fast_forward_pushButton->setText(FFWD_CHAR); ui->fast_backward_pushButton->setText(FBWD_CHAR);
    if(player!=nullptr) set_playpause_button_icon(player->state());
    else set_playpause_button_icon(VideoPlayer::PlayerState::StopState);
}
void ThumbnailerDialog::set_playpause_button_icon(int state){
    if(static_cast<VideoPlayer::PlayerState>(state)==VideoPlayer::PauseState) ui->playpause_pushButton->setText(PAUSE_CHAR);
    else ui->playpause_pushButton->setText(PLAY_CHAR);
}
bool ThumbnailerDialog::eventFilter(QObject *watched,QEvent *event){
    if(watched==ui->video_widget){
        if(event->type()==QEvent::DragEnter){
            //[[1]]: 鼠标进入label时, label接受拖放的动作, 无论有没有拖拽文件都会触发这个
            QDragEnterEvent *dee=static_cast<QDragEnterEvent*>(event);
            dee->acceptProposedAction();
            return true;
        }
        else if(event->type()==QEvent::Drop){
            //[[2]]: 当放操作发生后, 取得拖放的数据
            QDropEvent *de=static_cast<QDropEvent*>(event);
            QList<QUrl> urls=de->mimeData()->urls();
            if(urls.isEmpty()){return true;} //qDebug()<<urls;
            QString path=urls.first().toLocalFile();
            //[[3]]: Do Something
            set_video(path);
        }
        // else if(event->type()==QEvent::MouseButtonDblClick){
        //     QFileInfo vinfo(ui->current_video_lineEdit->text());
        //     if(vinfo.exists()){
        //         QByteArray tmpArr=QString("start /b \"\" \""+vinfo.absoluteFilePath()+"\"").toLocal8Bit();
        //         system(tmpArr.data());
        //     }
        // }
    }
    return QDialog::eventFilter(watched,event);
}
void ThumbnailerDialog::keyPressEvent(QKeyEvent *event){
    if(event->key()==Qt::Key_Z){
        player->prev_frame();
    }
    else if(event->key()==Qt::Key_X){
        player->next_frame();
    }
    else if(event->key()==Qt::Key_Right){
        qDebug()<<"incomplete function: forward";
    }
    else if(event->key()==Qt::Key_Left){
        qDebug()<<"incomplete function: backward";
    }
    else QDialog::keyPressEvent(event);
}
void ThumbnailerDialog::showEvent(QShowEvent *event){
    Q_UNUSED(event);
}
void ThumbnailerDialog::resizeEvent(QResizeEvent* event){
    Q_UNUSED(event); //暂时应该不需要
    ui->video_widget_info_label->resize(ui->video_widget->size());
    player->refresh_output_widget_size();
    emit dialog_resize_signal(this->size());
}
//注册一个全局热键. 如果该名称已注册过,会先注销旧的再注册新的
//注册成功后会同时记录到两个哈希表中,分别用于按名称查找和按ID查找
void ThumbnailerDialog::set_global_hotkey(const QString &name,Qt::KeyboardModifiers modifiers,Qt::Key key){
    clear_global_hotkey(name); //如果同名热键已存在,先注销
    UINT nativeMod=qtModifiersToNative(modifiers);
    UINT nativeVk=qtKeyToNativeVk(key);
    if(nativeVk==0){
        qWarning()<<"Unsupported key for global hotkey:"<<key;
        return;
    }
    int id=nextHotkeyId++;
    if(RegisterHotKey((HWND)winId(),id,nativeMod,nativeVk)){
        registeredHotkeys[name]={id,nativeMod,nativeVk}; //记录: 名称->信息
        hotkeyIdToName[id]=name;                          //记录: ID->名称
        qDebug().noquote()<<QKeySequence(modifiers|key).toString()<<"registered as global hotkey"<<QString("\"%1\"").arg(name);
    }
    else qWarning()<<"Failed to register global hotkey"<<name<<", error:"<<GetLastError();
}
void ThumbnailerDialog::clear_global_hotkey(const QString &name){
    auto it=registeredHotkeys.find(name);
    if(it!=registeredHotkeys.end()){
        UnregisterHotKey((HWND)winId(),it->id);
        hotkeyIdToName.remove(it->id);
        registeredHotkeys.erase(it);
    }
}
//拦截Windows原生消息. 当收到WM_HOTKEY时,不立即执行动作,而是启动定时器等待按键松开
//原因: 如果立即执行,槽函数内部可能会模拟按键(如PotPlayer的position()会模拟G键),
//      此时Ctrl+S还没松开,就变成了Ctrl+S+G三键同按,导致第三方播放器触发了错误的功能
bool ThumbnailerDialog::nativeEvent(const QByteArray &eventType,void *message,qintptr *result){
    if(eventType=="windows_generic_MSG"){
        MSG *msg=static_cast<MSG*>(message);
        if(msg->message==WM_HOTKEY){
            //wParam就是RegisterHotKey时传入的id,用它反查是哪个热键
            auto it=hotkeyIdToName.find((int)msg->wParam);
            if(it!=hotkeyIdToName.end()){
                pendingHotkeyAction=it.value(); //记下要执行的动作名称,等按键松开后再执行
                if(!hotkeyReleaseTimer){
                    hotkeyReleaseTimer=new QTimer(this);
                    hotkeyReleaseTimer->setInterval(20); //每20ms检测一次
                    connect(hotkeyReleaseTimer,&QTimer::timeout,this,&ThumbnailerDialog::check_hotkey_release);
                }
                hotkeyReleaseTimer->start();
                if(result) *result=0;
                return true;
            }
        }
    }
    return QDialog::nativeEvent(eventType,message,result);
}
//定时器回调: 用GetAsyncKeyState检测热键涉及的每个物理键是否已松开
//0x8000位表示该键当前是否被按下,全部松开后才执行dispatch
void ThumbnailerDialog::check_hotkey_release(){
    auto it=registeredHotkeys.find(pendingHotkeyAction);
    if(it==registeredHotkeys.end()){
        hotkeyReleaseTimer->stop();
        return;
    }
    const auto &e=it.value();
    if((e.nativeMod&MOD_CONTROL)&&(GetAsyncKeyState(VK_CONTROL)&0x8000)) return; //Ctrl还没松开
    if((e.nativeMod&MOD_ALT)&&(GetAsyncKeyState(VK_MENU)&0x8000)) return;       //Alt还没松开
    if((e.nativeMod&MOD_SHIFT)&&(GetAsyncKeyState(VK_SHIFT)&0x8000)) return;    //Shift还没松开
    if((e.nativeMod&MOD_WIN)&&((GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000))) return;
    if(GetAsyncKeyState(e.nativeVk)&0x8000) return; //主键还没松开
    //全部松开,执行动作
    hotkeyReleaseTimer->stop();
    dispatch_global_hotkey(pendingHotkeyAction);
}
//根据热键名称调用对应的槽函数. 如果新增其他全局热键,在这里添加 else if 分支即可
void ThumbnailerDialog::dispatch_global_hotkey(const QString &name){
    if(name=="snap") on_snap_pushButton_clicked();
}
void ThumbnailerDialog::closeEvent(QCloseEvent* event){
    Q_UNUSED(event);
    if(player->state()!=VideoPlayer::StopState){
        player->stop(); //event->accept();
    } //没有在播放视频, 直接退出
//    emit closed();
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
    snapped_image_list.clear();
    thumbnailer.set_video(video_path);
}

void ThumbnailerDialog::state_changed_slot(int state){
    switch(state){
    case VideoPlayer::StopState:{
        ui->video_widget_info_label->setVisible(true);
        //将播放器回归到初始状态,以便再次播放
        ui->volume_comboBox->setCurrentIndex(5); //100% volume
        ui->play_spd_comboBox->setCurrentIndex(3); //1.0 speed
        break;
    }
    case VideoPlayer::PlayState:{
        ui->video_widget_info_label->setVisible(false);
        break;
    }
    case VideoPlayer::PauseState:{
        ui->video_widget_info_label->setVisible(false);
        break;
    }
    default:{
        qWarning("undealed player state: %d",state);
    }
    }
    set_playpause_button_icon(state);
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
    emit thumbs_progress_changed(rate);
}

void ThumbnailerDialog::item_clicked(QModelIndex index){
    if(QMessageBox::Yes==QMessageBox::information(this,"提示","是否要删除刚才点击的项目?",QMessageBox::Yes,QMessageBox::No)){
        snaplist_model->removeRow(index.row()); update_thumbs_cnt_label();
        //use_snapped_image
        if(use_snapped_image){
            int v_index=index.row();
            if(v_index>=0&&v_index<(int)snapped_image_list.size()) snapped_image_list.remove(v_index);
            else qWarning()<<"index:"<<v_index<<"out of snapped_image_list's bound, delete Failed.";
        }
    }
}
void ThumbnailerDialog::grab_gif_image(){
    if(player==nullptr) return;
    gif_image_list.append(QImage(player->current_image())); //拷贝构造,深复制
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
void ThumbnailerDialog::on_next_frame_pushButton_clicked(){
    player->next_frame();
}
void ThumbnailerDialog::on_prev_frame_pushButton_clicked(){
    player->prev_frame();
}
void ThumbnailerDialog::on_fast_backward_pushButton_clicked(){
    player->seek(player->position()-5*1000);
}
void ThumbnailerDialog::on_fast_forward_pushButton_clicked(){
    player->seek(player->position()+5*1000);
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
        long long pos=player->position();
        // ** 获取正在显示的图像 **
        // @法1. 截取依附的窗口
        // QImage snap_img=QApplication::primaryScreen()->grabWindow(ui->video_widget->winId()).toImage();
        // @法2. 成员函数
        QImage snap_img=player->current_image();
        QPixmap tmp_img=QPixmap::fromImage(snap_img.scaled(icon_size,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        snaplist_model->appendRow(new QStandardItem(QIcon(tmp_img),QTime(0,0,0).addMSecs(pos).toString("hh:mm:ss.zzz")));
        update_thumbs_cnt_label();
        ui->thumbs_listView->scrollToBottom(); //添加内容之后移动到结尾

        //use_snapped_image
        if(use_snapped_image) snapped_image_list.push_back(QImage(snap_img));
    }
}
void ThumbnailerDialog::on_clear_thumbs_pushButton_clicked(){ //清空ListView
    if(QMessageBox::Yes==QMessageBox::information(this,"提示","删除所有手动截取的位置?",QMessageBox::Yes,QMessageBox::No)){
        snaplist_model->clear(); update_thumbs_cnt_label();
        snapped_image_list.clear();
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
    //此函数不允许中途 return
    bool need_hide=hide_window_when_generating; //拷贝一份,防止运行时被修改
    if(need_hide) this->setHidden(true);
    QVector<long long> snappedList;
    for(int i=0;i<snaplist_model->rowCount();++i){
        QString str=snaplist_model->item(i)->text();
        long long msec=QTime(0,0,0).msecsTo(QTime::fromString(str,"hh:mm:ss.zzz"));
        snappedList.push_back(msec);
    }
    if(!use_snapped_image) snapped_image_list.clear();
    QThread *t_thread=new QThread;
    Thumbnailer *t_tber=new Thumbnailer;
    t_tber->set_video(thumbnailer.get_video_path()); //必须先设置video,否则设置的thumbs会被覆盖
    QStringList thumbs_list=thumbnailer.get_thumbnails_path('|').split('|');
    if(thumbs_list.size()!=2){
        qWarning()<<"Illegal thumbnails path, number splitted by '|'(2 is right):"<<thumbs_list.size();
    }
    t_tber->set_thumbs_dir(thumbs_list.first());
    t_tber->set_thumbs_name(thumbs_list.last());
    t_tber->moveToThread(t_thread);
    connect(this,&::ThumbnailerDialog::trigger_get_thumbnails,t_tber, //get_thumbnails函数有多个,要进行强制类型转换
            static_cast<bool(Thumbnailer::*)(int,int,const QVector<long long>&,const QVector<QImage>&)>(&Thumbnailer::get_thumbnails));
    connect(t_tber,&Thumbnailer::thumbs_progress_changed,this,&::ThumbnailerDialog::thumbs_progress_changed_slot);
    t_thread->start();
    if(player->state()!=VideoPlayer::StopState) player->stop(); //停止播放当前视频,并且生成完后不恢复
    set_ui_state(false); //冻结界面，防止胡乱操作
    //当视频文件是.ts格式的时候，强制使用slow_algorithm
    if(t_tber->get_video_path().endsWith(".ts",Qt::CaseInsensitive)) t_tber->SlowThumbnailsAlgorithm=true;
    t_tber->get_thumbnails_result=-1;
    emit trigger_get_thumbnails(ui->row_spinBox->value(),ui->column_spinBox->value(),snappedList,snapped_image_list);
    while(t_tber->get_thumbnails_result==-1){
        QCoreApplication::processEvents();
    }
    bool res=t_tber->get_thumbnails_result==false?false:true; //执行完result只有t/f两种状态吧..
    //bool res=thumbnailer.get_thumbnails(ui->row_spinBox->value(),ui->column_spinBox->value(),snappedList);
    t_thread->quit(); t_thread->wait(); delete t_thread; //删除资源
    set_ui_state(true); //解冻界面
    if(need_hide) this->setHidden(false);
    QString tb_path=t_tber->get_thumbnails_path(); delete t_tber;
    if(exit_after_generate){
        if(res){
            QMessageBox::information(this,"Tips","图片生成完成, 即将退出\n保存路径: "+tb_path);
            player->stop(); this->accept();
        }
        else QMessageBox::warning(this,"Error","生成图片失败, 详情请参阅日志");
    }
    if(res) emit thumbnails_generated(tb_path);
    ui->get_thumbs_progressBar->setValue(0);
}
void ThumbnailerDialog::on_gif_recording_pushButton_clicked(){
    if(!is_recording_gif){
        if(QMessageBox::question(this,"","开始录制 GIF?")!=QMessageBox::Yes) return;
    }

    if(!is_recording_gif){ //开始录制
        gif_image_list.clear();
        gif_grab_timer=new QTimer(this);
        connect(gif_grab_timer,&QTimer::timeout,this,&::ThumbnailerDialog::grab_gif_image);
        int gif_fps=ui->gif_fps_spinBox->value();
        gif_grab_timer->start(1000/gif_fps);
        ui->gif_recording_pushButton->setText("End Recording");
        is_recording_gif=true;
    }
    else{ //已经在录制,结束录制
        gif_grab_timer->stop();
        int GIFMaxWidth=ui->max_gif_width_spinBox->value();
        int GIFMaxHeight=ui->max_gif_height_spinBox->value();
        QString gif_filepath;
        if(!gif_image_list.isEmpty()){
            //根据第一张图片的大小计算整个GIF图片的大小
            QSize img_size(gif_image_list.first().width(),gif_image_list.first().height());
            if(img_size.width()>GIFMaxWidth||img_size.height()>GIFMaxHeight){ //超过最大大小就缩放
                double fact_w=(double)GIFMaxWidth/img_size.width();
                double fact_h=(double)GIFMaxHeight/img_size.height();
                if(fact_w<fact_h) img_size=QSize((int)(img_size.width()*fact_w),(int)(img_size.height()*fact_w));
                else img_size=QSize((int)(img_size.width()*fact_h),(int)(img_size.height()*fact_h));
            }


            GifEncoder encoder;
            //获取写入GIF文件的地址
            gif_filepath=thumbnailer.get_thumbnails_path().replace(".png",".gif");
            encoder.open(gif_filepath,img_size.width(),img_size.height());
            int dly=gif_grab_timer->interval();
            int progress=0; //生成GIF图片的进度
            for(int i=0;i<(int)gif_image_list.size();++i){
                //qDebug()<<"write gif image:"<<i;
                QImage t_image=gif_image_list[i].scaled(img_size,Qt::IgnoreAspectRatio);
//                t_image=t_image.rgbSwapped(); //需要反色才是正常的效果,不知道为什么 //2026-04-27: bug 已修复
                encoder.push(t_image,dly); //dly/3+1 经验数据 //2026-04-27: 原先是把厘秒当毫秒了，现在已修复
                progress=(i+1)*100/((int)gif_image_list.size());
                ui->gif_recording_pushButton->setText("Progress: "+QString::number(progress)+"%");
                QCoreApplication::processEvents(); //防止界面阻塞
            }
            encoder.close();
            qDebug()<<"Debug: Save Gif at:"<<gif_filepath;
        }
        else qWarning()<<"End Recording GIF, But No Image Grabbed.";

        delete gif_grab_timer;
        gif_image_list.clear();
        ui->gif_recording_pushButton->setText("Record GIF");
        is_recording_gif=false;
        if(!gif_filepath.isEmpty()) emit thumbnails_generated(gif_filepath);
    }
}
void ThumbnailerDialog::on_slow_algo_checkBox_clicked(bool checked){Thumbnailer::DefaultSlowThumbnailsAlgorithm=checked;}
void ThumbnailerDialog::on_no_watermark_checkBox_clicked(bool checked){Thumbnailer::DefaultRemoveThumbnailsMark=checked;}
