#include "ExampleWidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ExampleWidget w;
    w.show();
    return a.exec();
}
