#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <yocto_motor.h>
#include <yocto_voltage.h>
#include <yocto_api.h>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QCloseEvent>
#include <qcustomplot.h>
#include <QTimer>

namespace Ui {
class MainWindow;
}

#define MIN_ZOOM_X 2.0

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    static void deviceArrived(YModule *m);
    static void deviceRemoved(YModule *m);
    static void CBVolt(YVoltage* f, const string &v);
    void initPlot();

public:
    enum idDevice{idYKnob      = 10,
                  idYWatchdog   = 51,
                  idYClusterLed = 101,
                  idY3d         = 106,
                  idMotor       = 22};

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void voltageChanged(double v);
    void borneRangeXAxis(QCPRange newR, QCPRange oldR);
    void updateRangeYAxis(QCPRange);
    void now();
    void recorded(bool b);
    void updateCurrentVoltageText(double v);

protected:
    void closeEvent(QCloseEvent*);

signals:
    void voltageChange(double v);

private:
    QCustomPlot *g;
    QCPGraph *t;
    static MainWindow* _mainWindow;
    Ui::MainWindow *ui;
    YMotor *_moteur;
    YVoltage *_volt;
    QFuture<void> watch;
    bool run,_now;
    QTimer timerVolt;
    QTime time;
    double _lastTime;
    double _decalage;

    QCPItemTracer *currentVoltageTracer;
    QCPItemText *currentVoltageText;
    QCPItemCurve *voltageTracerArrow;

};

#endif // MAINWINDOW_H
