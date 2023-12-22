#include<QApplication>
#include"thumbnailer.h"

int main(int argc, char *argv[]){
    QApplication a(argc, argv);
    ThumbnailerDialog w;
    w.show();
    return a.exec();
}
