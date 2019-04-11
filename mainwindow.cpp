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

    connect(this,       SIGNAL(voltageChange(double)),      this, SLOT(voltageChanged(double)));
    connect(ui->now,    SIGNAL(clicked(bool)),              this, SLOT(now()));
    connect(ui->record, SIGNAL(clicked(bool)),              this, SLOT(recorded(bool)));
    connect(ui->graph,  SIGNAL(mousePress(QMouseEvent*)),   this, SLOT(mousePressedGraph(QMouseEvent*)));
    connect(ui->graph,  SIGNAL(mouseMove(QMouseEvent*)),    this, SLOT(mousePressedGraph(QMouseEvent*)));
    connect(ui->graph, SIGNAL(mouseDoubleClick(QMouseEvent*)), this, SLOT(mouseDblClik(QMouseEvent*)));

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
    updateCurrentVoltageText(v);
    if(ui->record->isChecked()){
        double key = time.elapsed() /1000.0;
        key += _decalage;
        if(key - _lastTime > 0.02)
        {
            t->addData(key, v);
            _lastTime = key;
            currentVoltageTracer->setGraphKey(g->xAxis->range().upper);
            updateRangeYAxis(QCPRange());
           // g->replot();
            if(_now)
                now();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent*)
{
    run = false;
    watch.waitForFinished();
}

// double right click to clear measure
void MainWindow::mouseDblClik(QMouseEvent *e)
{
    if(e->button() == Qt::RightButton)
    {
        marker1->setVisible(false);
        marker2->setVisible(false);
        bracket->setVisible(false);
        timeMeasure->setVisible(false);
        textMeasure->setVisible(false);

        g->replot();
    }
}

void MainWindow::mousePressedGraph(QMouseEvent *e)
{
    if(t->dataCount() < 5)
        return;

    if(_now && (e->type()|QMouseEvent::MouseMove) && (e->buttons()&Qt::LeftButton))
        _now = false;

    if((e->type() | QMouseEvent::MouseButtonPress | QMouseEvent::MouseMove) &&
            e->buttons() & Qt::RightButton){
        double key,value;
        t->pixelsToCoords(e->pos(),key,value);

        //find more close to point
        QCPItemTracer * marker;

        if(e->type() == QMouseEvent::MouseButtonPress){
            if(qAbs(marker1->position->key()-key)<qAbs(marker2->position->key()-key))
                marker = marker1;
            else
                marker = marker2;

            if(!marker1->visible())
                marker = marker1;

            if(marker1->visible() && !marker2->visible())
                marker = marker2;
        }else{
            marker = markerSelected;
        }

        markerSelected = marker;
        marker->setGraphKey(key);
        marker->setVisible(true);

        g->replot(); // Need to be update to compute le next line !!!
        drawMeasure(marker->position->pixelPosition().y()/(qreal)t->valueAxis()->axisRect()->height());
        g->replot();
    }
}

void MainWindow::drawMeasure(qreal position)
{
    if(marker1->visible() && marker2->visible())
    {
        //si > 0.8 on dessine le braket au dessu du graph
        if(position >= 0.8)
        {
            if(posBracket->coords().y()!=0.1){
                posBracket->setCoords(0,0.1);
                timeMeasure->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
                timeMeasure->position->setCoords(0,-20);
            }

            if(marker1->position->key()<marker2->position->key()){
                if(bracket->left->parentAnchorX()!=marker1->position){
                    bracket->left->setParentAnchorX(marker1->position);
                    bracket->right->setParentAnchorX(marker2->position);
                }
            }else{
                if(bracket->left->parentAnchorX()!=marker2->position){
                    bracket->left->setParentAnchorX(marker2->position);
                    bracket->right->setParentAnchorX(marker1->position);
                }
            }
        }else{
            //sinon en dessous du graph
            if(posBracket->coords().y()!=0.8){
                posBracket->setCoords(0,0.8);
                timeMeasure->setPositionAlignment(Qt::AlignBottom|Qt::AlignHCenter);
                timeMeasure->position->setCoords(0,20);
            }

            if(marker1->position->key()>marker2->position->key()){
                if(bracket->left->parentAnchorX()!=marker1->position){
                    bracket->left->setParentAnchorX(marker1->position);
                    bracket->right->setParentAnchorX(marker2->position);
                }
            }else{
                if(bracket->left->parentAnchorX()!=marker2->position){
                    bracket->left->setParentAnchorX(marker2->position);
                    bracket->right->setParentAnchorX(marker1->position);
                }
            }
        }
        if(!bracket->visible())
            bracket->setVisible(true);

        //Compute time measured

        double interval = qAbs(marker1->position->key()-marker2->position->key());
        QTime timerValueMeasured(0,0,0,0);
        timerValueMeasured = timerValueMeasured.addMSecs(interval*1000.0);
        timeMeasure->setText(timerValueMeasured.toString(s_formatTime));
        showMeasureOutside();
        if(!timeMeasure->visible())
            timeMeasure->setVisible(true);
    }
}

void MainWindow::showMeasureOutside()
{
    if(!bracket->visible())
        return;
    if(g->axisRect()->rect().contains(bracket->center->pixelPosition().toPoint()))
    {
        if(textMeasure->visible())
            textMeasure->setVisible(false);
    }
    else{
        textMeasure->setText("Time = " + timeMeasure->text());
        if(!textMeasure->visible())
            textMeasure->setVisible(true);
    }
}

void MainWindow::now(){
    _now = true;

    QCPRange range = g->xAxis->range();
    //range.upper = (t->data().data()->end()-1)->key+1;
    //Test en %
    range.upper = (t->data().data()->end()-1)->key+  g->xAxis->range().size()*0.1;
    range.lower = range.upper - g->xAxis->range().size();
    g->xAxis->setRange(range.upper, range.size(), Qt::AlignRight);
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

*/
    g->xAxis->setRange((t->data().data()->end()-1)->key ,8,Qt::AlignRight);


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

    // now create measure
    marker1 = new QCPItemTracer(g);
    marker1->setGraph(t);
    marker1->setInterpolating(true);
    marker1->setStyle(QCPItemTracer::tsPlus);
    marker1->setPen(QPen(s_couleurMeasure));
    marker1->setSize(7);
    marker1->setVisible(false);

    marker2 = new QCPItemTracer(g);
    marker2->setGraph(t);
    marker2->setInterpolating(true);
    marker2->setStyle(QCPItemTracer::tsPlus);
    marker2->setPen(QPen(s_couleurMeasure));
    marker2->setSize(7);
    marker2->setVisible(false);


    bracket = new QCPItemBracket(g);
    posBracket = new QCPItemPosition(g,bracket,"position");
    posBracket->setAxisRect(g->axisRect());
    posBracket->setType(QCPItemPosition::ptAxisRectRatio);
    posBracket->setCoords(0,0.8);

    bracket->left->setParentAnchorX(marker1->position);
    bracket->left->setParentAnchorY(posBracket);
    bracket->right->setParentAnchorX(marker2->position);
    bracket->right->setParentAnchorY(posBracket);
    bracket->setPen(QPen(s_couleurMeasure));

    bracket->setVisible(false);

    // add label measure:
    timeMeasure = new QCPItemText(g);
    timeMeasure->position->setParentAnchor(bracket->center);
    timeMeasure->position->setCoords(0,20);
    timeMeasure->setPositionAlignment(Qt::AlignBottom|Qt::AlignHCenter);
    timeMeasure->setText("Temps : ?");
    timeMeasure->setTextAlignment(Qt::AlignCenter);
    s_fontUtsaah.setBold(true);
    timeMeasure->setFont(s_fontUtsaah);
    timeMeasure->setColor(s_couleurMeasure);
    timeMeasure->setVisible(false);

    textMeasure = new QCPItemText(g);
    textMeasure->position->setType(QCPItemPosition::ptAxisRectRatio);
    textMeasure->position->setCoords(0.3,0.9);
    textMeasure->setText("Temps : ?");
    textMeasure->setTextAlignment(Qt::AlignRight);
    textMeasure->setFont(s_fontUtsaah);
    textMeasure->setColor(s_couleurMeasure);
    textMeasure->setVisible(false);

    g->replot();


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

    showMeasureOutside();
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
    switch (m->get_productId()) {
    case MainWindow::idMotor: // Motor
        if(_mainWindow->_moteur != nullptr)
            return;
        _mainWindow->_moteur = yFindMotor(m->get_friendlyName());
        _mainWindow->_volt = yFindVoltage(m->get_friendlyName());
        qDebug() << "[YOCTO] Sonde de tension trouvée ?" << _mainWindow->_volt;
        _mainWindow->ui->record->setEnabled(true);
        _mainWindow->_volt->setResolution(0.01);
        _mainWindow->_volt->registerValueCallback(CBVolt);
        break;
    default:
        break;
    }
}

void MainWindow::deviceRemoved(YModule *m)
{
    switch (m->get_productId()) {
    case MainWindow::idMotor: // Motor
        if(_mainWindow->_moteur == nullptr)
            return;

        _mainWindow->ui->record->setEnabled(false);
        _mainWindow->_moteur = nullptr;
        _mainWindow->_volt = nullptr;
        _mainWindow->updateCurrentVoltageText(-100);
        qDebug() << "[YOCTO] Sonde de tension retiré ?" << _mainWindow->_volt;
        break;
    default:
        break;
    }
}

void MainWindow::CBVolt(YVoltage* f, const string &v)
{
    Q_UNUSED(f)
    emit _mainWindow->voltageChange(atof(v.data()));
}

MainWindow::~MainWindow()
{
    delete ui;
}
