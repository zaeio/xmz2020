#include "widget.h"
#include "stdio.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    printf("Loading...\n");

    QApplication a(argc, argv);
    Widget w;

    printf("Widget created\n");

    w.show();

    return a.exec();
}
