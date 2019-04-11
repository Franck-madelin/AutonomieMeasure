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
#include <QMouseEvent>
#include <qcustomplot.h>
#include <QTimer>

namespace Ui {
class MainWindow;
}

#define MIN_ZOOM_X 2.0
static QString  s_Address_IP = "192.168.1.10";
static QColor   s_couleurMeasure(32,155,230);
static QString  s_formatTime("HH'h' mm'm' ss's' zzz'ms'");
static QFont    s_fontUtsaah = QFont("Utsaah", 12);


class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    static void deviceArrived(YModule *m);
    static void deviceRemoved(YModule *m);
    static void CBVolt(YVoltage* f, const string &v);
    void initPlot();
    void drawMeasure(qreal position);
    void showMeasureOutside();

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
    void mouseDblClik(QMouseEvent *e);
    void mousePressedGraph(QMouseEvent* e);
    void clearGraph();



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
    QTime time;
    double _lastTime;
    double _decalage;

    QCPItemTracer *currentVoltageTracer,*marker1,*marker2,*markerSelected;
    QCPItemText *currentVoltageText,*timeMeasure,*textMeasure;
    QCPItemCurve *voltageTracerArrow;
    QCPItemBracket *bracket;
    QCPItemText *leftBracket,*rightBracket;
    QCPItemPosition *posBracket;

};

#endif // MAINWINDOW_H
