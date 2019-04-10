#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow* MainWindow::_mainWindow = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    g = ui->graph;
    _mainWindow = this;
    run = false;
    _now = true;
    _moteur = nullptr;
    _volt = nullptr;

    ui->record->setEnabled(false);

    initPlot();

    connect(this, SIGNAL(voltageChange(double)), this, SLOT(voltageChanged(double)));
    connect(ui->now, SIGNAL(clicked(bool)), this, SLOT(now()));
    connect(ui->record, SIGNAL(clicked(bool)), this, SLOT(recorded(bool)));


    yDisableExceptions();
    yRegisterDeviceArrivalCallback(deviceArrived);
    yRegisterDeviceRemovalCallback(deviceRemoved);

    string add = "192.168.1.10";
    string err;
    if (yRegisterHub(add, err) != YAPI_SUCCESS)
    {
        qDebug() << "API NOT REGISTRED";
        return;
    }

    watch = QtConcurrent::run([this](){
        run = true;
        string err;
        while(run)
        {
            //qDebug() << "UP YOCTO";
            yUpdateDeviceList(err);
            ySleep(800,err);
        }
    });
}

void MainWindow::voltageChanged(double v)
{
    // qDebug() << "Voltage = " << v << " V" << time.elapsed();
    updateCurrentVoltageText(v);
    if(ui->record->isChecked()){
        double key = time.elapsed() /1000.0;
        key += _decalage;
        if(key - _lastTime > 0.02)
        {
            //qDebug() << "ADD DATA" << key << v;
            t->addData(key, v);
            _lastTime = key;
            g->replot();
            now();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent*)
{
    run = false;
    watch.waitForFinished();
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::now(){
    QCPRange range = g->xAxis->range();
    //range.upper = (t->data().data()->end()-1)->key+1;
    //Test en %
    range.upper = (t->data().data()->end()-1)->key+  g->xAxis->range().size()*0.1;

    range.lower = range.upper - g->xAxis->range().size();
    g->xAxis->setRange(range.upper, range.size(), Qt::AlignRight);
    currentVoltageTracer->setGraphKey(g->xAxis->range().upper);
    t->rescaleValueAxis(false,true);
    g->replot();

}

void MainWindow::recorded(bool b)
{
    if(b){
        time = QTime(0,0,0,0);
        if(t->data().data()->isEmpty()){
            //qDebug() << "FIRST START";
            _lastTime = 0;
        }else{
            //qDebug() << "Last time " << _lastTime;
        }
        time.start();
    }else{
        _decalage = _lastTime;
    }
}

void MainWindow::updateCurrentVoltageText(double v)
{

    if(v == -100)
        currentVoltageText->setText("Tension: ?");
    else{
        qreal positionPixelPoint = (currentVoltageTracer->position->pixelPosition().y() - (t->valueAxis()->axisRect()->top()-1))/(qreal)t->valueAxis()->axisRect()->height();
        if(currentVoltageText->position->coords().y() != 0.9 && positionPixelPoint<=0.25){
            currentVoltageText->position->setCoords(1,0.9);
            voltageTracerArrow->start->setParentAnchor(currentVoltageText->top);
            voltageTracerArrow->start->setCoords(0,-5);
            voltageTracerArrow->startDir->setCoords(0,-30);

            voltageTracerArrow->end->setCoords(-10,10);
            voltageTracerArrow->endDir->setCoords(-30,30);
        }else if(currentVoltageText->position->coords().y() != 0 && positionPixelPoint>0.25){
            currentVoltageText->position->setCoords(1,0);
            voltageTracerArrow->start->setParentAnchor(currentVoltageText->bottom);
            voltageTracerArrow->start->setCoords(0,5);
            voltageTracerArrow->startDir->setCoords(0,30);

            voltageTracerArrow->end->setCoords(-10,-10);
            voltageTracerArrow->endDir->setCoords(-30,-30);
        }

        currentVoltageText->setText(QString("Tension: %1%2V").arg(v<10?"  ":"").arg(v,0,'f',2));
    }
    g->replot();
}

void MainWindow::initPlot()
{
    t = g->addGraph();// Voltage
    t->setPen(QPen(Qt::green));

    g->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%hh %mm %ss");
    g->xAxis->setTicker(timeTicker);
    g->xAxis->setTickLength(10);


    /*  for(int i=0; i<10 ;i++)
        t->addData(i, QRandomGenerator::global()->bounded(1,5));

    for(int i=0; i<80 ;i++)
        t->addData(10+i, QRandomGenerator::global()->bounded(1,10));

    for(int i=0; i<10 ;i++)
        t->addData(10+80+i, QRandomGenerator::global()->bounded(1,5));

    t->rescaleKeyAxis();
    t->rescaleValueAxis();
    g->yAxis->setRangeLower(0);


    g->xAxis->setRange((t->data().data()->end()-1)->key ,8,Qt::AlignRight);
*/


    /*Dot red to visualize current value*/
    // add the phase tracer (red circle) which sticks to the graph data (and gets updated in bracketDataSlot by timer event):
    currentVoltageTracer = new QCPItemTracer(g);
    currentVoltageTracer->setGraph(t);
    currentVoltageTracer->setGraphKey((t->data().data()->end()-1)->key);
    currentVoltageTracer->setInterpolating(true);
    currentVoltageTracer->setStyle(QCPItemTracer::tsCircle);
    currentVoltageTracer->setPen(QPen(Qt::red));
    currentVoltageTracer->setBrush(Qt::red);
    currentVoltageTracer->setSize(7);


    // add label for group tracer:
    currentVoltageText = new QCPItemText(g);
    currentVoltageText->position->setType(QCPItemPosition::ptAxisRectRatio);
    currentVoltageText->setPositionAlignment(Qt::AlignRight|Qt::AlignTop);
    currentVoltageText->position->setCoords(1.0, 0); // upper right corner of axis rect
    currentVoltageText->setText("Tension : ?");
    currentVoltageText->setTextAlignment(Qt::AlignRight);
    currentVoltageText->setFont(QFont("Utsaah", 9));
    currentVoltageText->setColor(Qt::red);


    // add arrow pointing at point tracer, coming from label:
    voltageTracerArrow = new QCPItemCurve(g);
    voltageTracerArrow->start->setParentAnchor(currentVoltageText->bottom);
    voltageTracerArrow->start->setCoords(0,5);
    voltageTracerArrow->startDir->setParentAnchor(voltageTracerArrow->start);
    voltageTracerArrow->startDir->setCoords(0,30);
    voltageTracerArrow->end->setParentAnchor(currentVoltageTracer->position);
    voltageTracerArrow->end->setCoords(-10,-10);
    voltageTracerArrow->endDir->setParentAnchor(voltageTracerArrow->end);
    voltageTracerArrow->endDir->setCoords(-30,-30);
    voltageTracerArrow->setHead(QCPLineEnding::esSpikeArrow);
    QCPLineEnding underlineText(QCPLineEnding::esBar, (currentVoltageText->right->pixelPosition().x()-currentVoltageText->left->pixelPosition().x())*1.2);
    voltageTracerArrow->setTail(underlineText);




    connect(g->xAxis, SIGNAL(rangeChanged(QCPRange,QCPRange)),  this, SLOT(borneRangeXAxis(QCPRange,QCPRange)));
    connect(g->yAxis, SIGNAL(rangeChanged(QCPRange)),           this, SLOT(updateRangeYAxis(QCPRange)));

}

void MainWindow::borneRangeXAxis(QCPRange newR, QCPRange oldR)
{
    if(t->dataCount()==0)
        return;

    //qDebug() << "range " << newR << newR.size() << (t->data().data()->end()-1)->key  ;

    if(t->dataCount()==1)
    {
        g->xAxis->setRange(QCPRange(0,5));
        return;
    }

    if(newR.size() < MIN_ZOOM_X)
        g->xAxis->setRange(oldR);

    if(newR.upper > (t->data().data()->end()-1)->key + newR.size()/2)
        g->xAxis->setRange(oldR);

    if(newR.upper < 0)
        g->xAxis->setRange(oldR);


    g->yAxis->setRangeLower(0);
}

void MainWindow::updateRangeYAxis(QCPRange)
{    
    if(t->dataCount()==0)
        return;

    g->yAxis->blockSignals(true);
    t->rescaleValueAxis(false,true);
    g->yAxis->setRangeLower(0);
    g->yAxis->blockSignals(false);
}


// CALL BACK YOCTOPUCE

void MainWindow::deviceArrived(YModule *m)
{
    //qDebug() << m->get_friendlyName().c_str();
    // qDebug() << m->get_productId();

    switch (m->get_productId()) {
    case MainWindow::idMotor: // Motor
        if(_mainWindow->_moteur != nullptr)
            return;
        _mainWindow->_moteur = yFindMotor(m->get_friendlyName());
        _mainWindow->_volt = yFindVoltage(m->get_friendlyName());
        //qDebug() << "[YOCTO] Carte Moteur connectée" << _mainWindow->_moteur;
        qDebug() << "[YOCTO] Sonde de tension trouvée ?" << _mainWindow->_volt;
        _mainWindow->ui->record->setEnabled(true);
        _mainWindow->_volt->setResolution(0.01);
        _mainWindow->_volt->registerValueCallback(CBVolt);
        break;
    default:
        break;
    }
}
//typedef void (*YVoltageTimedReportCallback)(YVoltage *func, YMeasure measure);

void MainWindow::deviceRemoved(YModule *m)
{
    switch (m->get_productId()) {
    case MainWindow::idMotor: // Motor
        //qDebug() << _mainWindow->_moteur;
        if(_mainWindow->_moteur == nullptr)
            return;

        _mainWindow->ui->record->setEnabled(false);
        _mainWindow->_moteur = nullptr;
        _mainWindow->_volt = nullptr;
        //qDebug() << "[YOCTO] Carte Moteur déconnectée" << _mainWindow->_moteur;
        _mainWindow->updateCurrentVoltageText(-100);
        qDebug() << "[YOCTO] Sonde de tension retiré ?" << _mainWindow->_volt;
        break;
    default:
        break;
    }
}

void MainWindow::CBVolt(YVoltage* f, const string &v)
{
    emit _mainWindow->voltageChange(atof(v.data()));
}
