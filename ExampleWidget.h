#ifndef EXAMPLEWIDGET_H
#define EXAMPLEWIDGET_H

#include <QWidget>
#include "SpecEngine.h"
class ExampleWidget : public QWidget
{
    Q_OBJECT

public:
    ExampleWidget(QWidget *parent = nullptr);
    ~ExampleWidget();
private:
    //需要使用频谱，只需创建一个Spectrum对象
    Spectrum spectrum;
protected:
    virtual void paintEvent(QPaintEvent *event) override;
};
#endif // EXAMPLEWIDGET_H
