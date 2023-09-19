#include<QApplication>
#include"thumbnails.h"

#include<QDir>

int main(int argc, char *argv[]){
    QApplication a(argc, argv);
    Thumbnails t;
    t.set_video("E:/Backup/Redmi K30Pro/Android/videos/from data/Series Movies/Bondage Tea/bondage tea 9.mp4");
    
    t.get_thumbnails(0,0);
    return 0;
    
    ThumbnailsDialog w;
    w.show();
    return a.exec();
}
