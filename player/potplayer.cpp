#include"potplayer.h"
#include<QFileInfo>
#include<QDir>
#include<QWidget>
#include<QElapsedTimer>
#include<QScreen>
#include<QBuffer>
#include<QMessageBox>
#include<QClipboard>
#include<QFileDialog>
#include<windows.h>
#include<QApplication> //for processEvents() & grab()


QString PotPlayer::PotPlayerPath=QStringLiteral("C:\\Program Files\\PotPlayer\\PotPlayerMini64.exe");


int PotPlayerBottomPxSize=124;
int PotPlayerTopPxSize=100;
int PotPlayerLeftPxSize=10;
int PotPlayerRightPxSize=10;


void PotPlayer::delay(int ms){ //延时,不能直接sleep延时,UI主线程不能直接被阻塞
    QElapsedTimer stopTime;
    stopTime.start();
    while(stopTime.elapsed()<ms){ //stopTime.elapsed()返回从start开始到现在的毫秒数
        QCoreApplication::processEvents();
    }
}


PotPlayer::PotPlayer(QObject *parent):VideoPlayer{parent}{
    position_changed_threshold_val=0;
    current_state=PlayerState::StopState;
    video_path.clear();
    process=nullptr; //非常重要!! 否则process可能会是野指针
}
PotPlayer::~PotPlayer(){ //貌似不需要加 del process;
}

bool PotPlayer::make_sure_potplayer_exists(){
    if(QFile::exists(PotPlayerPath)) return true;
    QString fpath=QFileDialog::getOpenFileName(nullptr,"选择 PotPlayer 路径","./","可执行文件(*.exe)");
    if(QFile::exists(fpath)){
        PotPlayerPath=fpath; return true;
    }
    return false;
}
void PotPlayer::play(){
    int lastState=current_state;
    current_state=PlayerState::PlayState;
    if(lastState!=current_state) emit state_changed(current_state);
    if(lastState==PlayerState::StopState){ //首次播放(Stop -> Play)
        potplayer_wid=(unsigned long long)start_potplayer(video_path);
        if(potplayer_wid==0) stop(); //播放失败
    }
}
void PotPlayer::pause(){
    int lastState=current_state;
    current_state=PlayerState::PauseState;
    if(lastState!=current_state) emit state_changed(current_state);
}
void PotPlayer::stop(){
    if(current_state==PlayerState::StopState) return;
    int lastState=current_state;
    current_state=PlayerState::StopState;
    if(lastState!=current_state) emit state_changed(current_state);
    if(process!=nullptr){
        process->close();
        delete process;
        potplayer_wid=0;
    }
}
VideoPlayer::PlayerState PotPlayer::state(){
    active_potplayer(); //尝试将焦点置于PotPlayer
    return current_state;
}
bool PotPlayer::set_video(const QString &filename){
    if(!QFileInfo::exists(filename)&&!filename.isEmpty()) return false;
    video_path=filename;
    emit video_changed(video_path);
    stop();
    return true;
}
long long PotPlayer::position(){
    if(potplayer_wid==0){
        qWarning()<<"Get position when PotPlayer WId is null.";
        return 0;
    }
    active_potplayer(50); //将焦点设置到 PotPlayer 上
    // 模拟按下G键
    keybd_event('G',(BYTE)0,0,0);
    keybd_event('G',(BYTE)0,KEYEVENTF_KEYUP,0);
    delay(100); //必须要延时,等待子窗口弹出
    // 模拟按下 Ctrl+C
    keybd_event(VK_CONTROL,(BYTE)0,0,0);
    keybd_event('C',(BYTE)0,0,0);
    delay(20);
    keybd_event('C',(BYTE)0,KEYEVENTF_KEYUP,0);
    keybd_event(VK_CONTROL,(BYTE)0,KEYEVENTF_KEYUP,0);

    // 获取剪切板对象
    QClipboard *clipboard=QApplication::clipboard();
    // 从剪切板获取选中的内容
    QString s=clipboard->text();
    long long pos=-QTime::fromString(s.simplified(),"hh:mm:ss.zzz").msecsTo(QTime(0,0));
    /*
     * 清空刚才复制的那一条,尽管clear()应该是全部清空,但不知为什么其它项没有受影响
     * QClipboard::Clipboard 全局 ::Selection 全局鼠标选择(例如 X11) ::FindBuffer
    */
    clipboard->clear();

    // 模拟按下ESC键
    keybd_event(VK_ESCAPE,(BYTE)0,0,0);
    keybd_event(VK_ESCAPE,(BYTE)0,KEYEVENTF_KEYUP,0);

    //pos+=10; //修正值

    return pos;
}
void PotPlayer::set_position_changed_threshold(int gap_ms){
    position_changed_threshold_val=gap_ms;
}
int PotPlayer::position_changed_threshold(){
    return position_changed_threshold_val;
}
void PotPlayer::set_output_widget(QWidget *output_widget){
    this->potplayer_output_widget=output_widget;
}
const QWidget *PotPlayer::output_widget(){
    return potplayer_output_widget;
}
QString PotPlayer::video(){
    return video_path;
}

QImage PotPlayer::current_image(){
    if(potplayer_wid==0){
        qWarning()<<"Get current_image when PotPlayer WId is null.";
        return QImage();
    }
    QImage curImg=QApplication::primaryScreen()->grabWindow(potplayer_wid).toImage();
    //计算裁剪后的尺寸
    int croppedWidth=curImg.width()-PotPlayerLeftPxSize-PotPlayerRightPxSize;
    int croppedHeight=curImg.height()-PotPlayerTopPxSize-PotPlayerBottomPxSize;
    //裁剪图像
    QRect cropRect(PotPlayerLeftPxSize,PotPlayerTopPxSize,croppedWidth,croppedHeight);
    /*
     * 经试验,需要先将图片进行保存再载入才是正确的图像
     * 否则尽管图像的尺寸数据没错，但绘制的时候右下角会是黑色没有内容，需要缩放到两倍才是正确图像
     * 原因可能和系统设置了200%的缩放有关？（未确定 2024-06-24）
    */
    //将QImage保存到内存中
    QByteArray byteArray; QBuffer buffer(&byteArray); buffer.open(QIODevice::WriteOnly);
    curImg.copy(cropRect).save(&buffer,"PNG"); buffer.close();
    //从内存中加载QImage
    curImg.loadFromData(byteArray,"PNG");
    return curImg;
    //return curImg.copy(cropRect);
}
QString PotPlayer::video_player_name(){return QStringLiteral("PotPlayer");}
unsigned long long PotPlayer::start_potplayer(const QString &vpath){
    if(!make_sure_potplayer_exists()){
        qCritical()<<"无法启动 PotPlayer.exe, 无效的路径."; return 0;
    }
    process=new QProcess(this);
    process->start(PotPlayerPath,QStringList()<<QDir::toNativeSeparators(QFileInfo(vpath).absoluteFilePath()));
    QMessageBox(QMessageBox::Information,"Info","请在 PotPlayer 开始播放后关闭此窗口",QMessageBox::Yes,nullptr,Qt::WindowStaysOnTopHint).exec();
    long long pId=process->processId(); wchar_t szBuf[256];
    HWND hWnd=GetTopWindow(GetDesktopWindow());
    WId hWnd_WId=(WId)0; //WId a.k.a unsigned long long
    //qDebug()<<"--------"<<"pid:"<<pId;
    while(hWnd){
        DWORD wndProcID=0;
        GetWindowThreadProcessId(hWnd,&wndProcID);
        if(wndProcID==pId){
            GetClassNameW(hWnd,szBuf,sizeof(szBuf));
            if(QString::fromWCharArray(szBuf).contains("PotPlayer",Qt::CaseInsensitive)){ //PotPlayer64
                hWnd_WId=(WId)hWnd;
                break;
            }
        }
        hWnd=GetNextWindow(hWnd,GW_HWNDNEXT);
    }
    // 下面的代码是尝试将PotPlayer置于Dialog之前,但会拦截掉输入到PotPlayer的按键
    // if(output_widget!=nullptr&&output_widget->parentWidget()!=nullptr){
    //     //这两个QWindow*没有new,不需要释放
    //     QWindow *potplayer_window=QWindow::fromWinId(hWnd_WId);
    //     QWindow *output_widget_window=output_widget->parentWidget()->windowHandle();
    //     potplayer_window->setParent(output_widget_window);
    //     //potplayer_window->setFlags(potplayer_window->flags()|Qt::Window);
    // }
    return (unsigned long long)hWnd_WId;
}
void PotPlayer::active_potplayer(int delay_ms){
    if(potplayer_wid>0){
        SetForegroundWindow((HWND)potplayer_wid);
        SetFocus((HWND)potplayer_wid);
        if(delay_ms>0) delay(delay_ms);
    }
}




void PotPlayer::seek(long long pos){
    Q_UNUSED(pos);
    //qDebug()<<"unimplement-func:"<<__func__;
}
long long PotPlayer::duration(){
    qDebug()<<"unimplement-func:"<<__func__;
    return 0;
}
int PotPlayer::width(){
    qDebug()<<"unimplement-func:"<<__func__;
    return 0;
}
int PotPlayer::height(){
    qDebug()<<"unimplement-func:"<<__func__;
    return 0;
}
int PotPlayer::volume(){
    qDebug()<<"unimplement-func:"<<__func__;
    return 0;
}
void PotPlayer::set_volume(int volume){
    Q_UNUSED(volume);
    //qDebug()<<"unimplement-func:"<<__func__;
}
bool PotPlayer::mute(){
    qDebug()<<"unimplement-func:"<<__func__;
    return false;
}
void PotPlayer::set_mute(bool muted){
    Q_UNUSED(muted);
    //qDebug()<<"unimplement-func:"<<__func__;
}
double PotPlayer::speed(){
    qDebug()<<"unimplement-func:"<<__func__;
    return 0;
}
void PotPlayer::set_speed(double speed){
    Q_UNUSED(speed);
    //qDebug()<<"unimplement-func:"<<__func__;
}
void PotPlayer::refresh_output_widget_size(){
    //qDebug()<<"unimplement-func:"<<__func__;
}
