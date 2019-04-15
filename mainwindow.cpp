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
    timerClearBeacon.setSingleShot(true);
    timerClearBeacon.setInterval(500);
    menuRemove = ui->menuRemove;
    menuRemove->setVisible(false);

    ui->record->setEnabled(false);

    initPlot();

    connect(this,       SIGNAL(voltageChange(double)),      this, SLOT(voltageChanged(double)));
    connect(this,       SIGNAL(beaconChange(bool)),         this, SLOT(moteurBeaconChanged(bool)));
    connect(ui->now,    SIGNAL(clicked(bool)),              this, SLOT(now()));
    connect(ui->record, SIGNAL(clicked(bool)),              this, SLOT(recorded(bool)));
    connect(ui->clear,  SIGNAL(clicked(bool)),              this, SLOT(clearGraph()));
    connect(g,          SIGNAL(mousePress(QMouseEvent*)),   this, SLOT(mousePressedGraph(QMouseEvent*)));
    connect(g,          SIGNAL(mouseMove(QMouseEvent*)),    this, SLOT(mousePressedGraph(QMouseEvent*)));
    connect(g,          SIGNAL(mouseDoubleClick(QMouseEvent*)), this, SLOT(mouseDblClik(QMouseEvent*)));

    connect(ui->actionInsertLegend, SIGNAL(triggered(bool)), this, SLOT(insertLegend()));

    yDisableExceptions();
    yRegisterDeviceArrivalCallback(deviceArrived);
    yRegisterDeviceRemovalCallback(deviceRemoved);

    string add = s_Address_IP.toStdString();
    string err;
     if (yRegisterHub(add, err) == YAPI_SUCCESS)
    {
        qDebug() << "API REGISTRED";

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
    }else{
        qDebug() << "API NOT REGISTRED";
    }
}

void MainWindow::insertLegend()
{
    if(t->dataCount()==0)
        return;

    QString input = QInputDialog::getText(this, "Add Text to Legend", "Ajouter un texte à la légende:");

    if(input.isEmpty())
        return;

    QCPTextElement *tmp = new QCPTextElement(g);
    tmp->setText(input);
    tmp->setFont(s_fontUtsaah);
    tmp->setTextColor(s_colorGraph);
    tmp->setTextFlags(Qt::AlignCenter);
    tmp->setLayer(g->legend->layer());

    if(!g->legend->addElement(g->legend->elementCount(),0,tmp))
        return;

    QAction* item = new QAction(menuRemove);
    item->setText((input.length()>8)?input.left(8)+"...":input);
    item->setData(QVariant::fromValue<QCPTextElement*>(tmp));
    connect(item, SIGNAL(triggered(bool)), this, SLOT(removeItemLegend()));

    menuRemove->addAction(item);

    g->legend->setVisible(true);
    g->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignHCenter);
    g->replot();
}

void MainWindow::removeItemLegend()
{

    QAction * a = static_cast<QAction*>(sender());
    QCPTextElement *item = a->data().value<QCPTextElement*>();
    g->legend->remove(item);
    menuRemove->removeAction(a);

    if(menuRemove->actions().size() == 0)
        g->legend->setVisible(false);

    g->replot();
}

void MainWindow::voltageChanged(double v)
{
    updateCurrentVoltageText(v);
    if(ui->record->isChecked()){
        double key = time.elapsed() /1000.0;
        key += _decalage;
        if(key - _lastTime > 0.02)
        {
            //qDebug() << "add data";
            t->addData(key, v);
            _lastTime = key;
            currentVoltageTracer->setGraphKey((t->data().data()->end()-1)->key);
            //g->replot();
            if(_now)
                now();
        }
    }

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
        leftBracket->setVisible(false);
        rightBracket->setVisible(false);

        g->replot();
    }
}

void MainWindow::mousePressedGraph(QMouseEvent *e)
{
    if(t->dataCount() < 5)
        return;

    if(_now && (e->type()|QMouseEvent::MouseMove) && (e->buttons()&Qt::LeftButton))
    {
        _now = false;
        ui->now->setChecked(false);
    }

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

                leftBracket->setPositionAlignment(Qt::AlignBottom|Qt::AlignHCenter);
                rightBracket->setPositionAlignment(Qt::AlignBottom|Qt::AlignHCenter);
                leftBracket->position->setCoords(0,20);
                rightBracket->position->setCoords(0,20);
            }

            if(marker1->position->key()<marker2->position->key()){
                leftBracket->setText(QString("%1 V").arg(marker1->position->value(),0,'f',2));
                rightBracket->setText(QString("%1 V").arg(marker2->position->value(),0,'f',2));

                if(bracket->left->parentAnchorX()!=marker1->position){
                    bracket->left->setParentAnchorX(marker1->position);
                    bracket->right->setParentAnchorX(marker2->position);
                }
            }else{
                leftBracket->setText(QString("%1 V").arg(marker2->position->value(),0,'f',2));
                rightBracket->setText(QString("%1 V").arg(marker1->position->value(),0,'f',2));

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
                leftBracket->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
                rightBracket->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
                leftBracket->position->setCoords(0,-20);
                rightBracket->position->setCoords(0,-20);
            }

            if(marker1->position->key()>marker2->position->key()){
                leftBracket->setText(QString("%1 V").arg(marker1->position->value(),0,'f',2));
                rightBracket->setText(QString("%1 V").arg(marker2->position->value(),0,'f',2));

                if(bracket->left->parentAnchorX()!=marker1->position){
                    bracket->left->setParentAnchorX(marker1->position);
                    bracket->right->setParentAnchorX(marker2->position);
                }
            }else{
                leftBracket->setText(QString("%1 V").arg(marker2->position->value(),0,'f',2));
                rightBracket->setText(QString("%1 V").arg(marker1->position->value(),0,'f',2));


                if(bracket->left->parentAnchorX()!=marker2->position){
                    bracket->left->setParentAnchorX(marker2->position);
                    bracket->right->setParentAnchorX(marker1->position);
                }
            }
        }

        if(!bracket->visible())
        {
            bracket->setVisible(true);
            leftBracket->setVisible(true);
            rightBracket->setVisible(true);
        }

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

    _now = ui->now->isChecked();

    if(!_now)
        return;

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
            _decalage = 0;
        }
        ui->now->setChecked(true);
        now();
        _moteur->module()->setBeacon(Y_BEACON_ON);
        time.start();
    }else{
        _decalage = _lastTime;
        _moteur->module()->setBeacon(Y_BEACON_OFF);
    }
}

void MainWindow::moteurBeaconChanged(bool b)
{
    if(!b && timerClearBeacon.isActive())
    {
        _moteur->module()->muteValueCallbacks();
        ui->record->setChecked(false);
        clearGraph();
        _moteur->module()->unmuteValueCallbacks();
        return;
    }

    if(b)
        timerClearBeacon.start(500);


    if(b && !ui->record->isChecked()) // Start recording
    {
        ui->record->setChecked(true);
        recorded(true);
    }else if(!b && ui->record->isChecked()){ // Pause recording
        ui->record->setChecked(false);
        recorded(false);
    }
}

void MainWindow::clearGraph()
{
    if(t->dataCount()==0)
        return;

    if(sender() != this){
        int rep = QMessageBox::warning(this, "Suppression graphique !", "Etes vous sur de supprimer le graphique ?",QMessageBox::Yes, QMessageBox::No);
        if(rep!=QMessageBox::Yes)
            return;
    }

    t->data().data()->clear();
    g->xAxis->setRange(0 ,8, Qt::AlignCenter);
    g->yAxis->setRange(0, 6);
    marker1->setVisible(false);
    marker2->setVisible(false);
    bracket->setVisible(false);
    timeMeasure->setVisible(false);
    textMeasure->setVisible(false);
    currentVoltageTracer->setVisible(false);
    voltageTracerArrow->setVisible(false);
    leftBracket->setVisible(false);
    rightBracket->setVisible(false);

    g->replot();
    recorded(ui->record->isChecked());
}

void MainWindow::updateCurrentVoltageText(double v)
{
    if(v == -100)
        currentVoltageText->setText("Tension: ?");
    else{
        if(!currentVoltageTracer->visible() && t->dataCount()!=0 ){
            currentVoltageTracer->setVisible(true);
            voltageTracerArrow->setVisible(true);
        }

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
    t->setName("Voltage");
    t->setPen(QPen(s_colorGraph,2));
    g->legend->removeAt(0);
    g->legend->setBorderPen(QPen(Qt::darkGreen));

    g->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%hh %mm %ss");
    timeTicker->setTickCount(6);
    g->xAxis->setTicker(timeTicker);
    g->xAxis->setTickLength(10);

    for(int i=0; i<10 ;i++)
        t->addData(i, QRandomGenerator::global()->bounded(1,5));

    for(int i=0; i<80 ;i++)
        t->addData(10+i, QRandomGenerator::global()->bounded(1,10));

    for(int i=0; i<10 ;i++)
        t->addData(10+80+i, QRandomGenerator::global()->bounded(1,5));

    t->rescaleKeyAxis();
    t->rescaleValueAxis();
    g->yAxis->setRangeLower(0);

    g->xAxis->setRange((t->data().data()->end()-1)->key ,8,Qt::AlignCenter);


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
    currentVoltageTracer->setVisible(false);

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
    voltageTracerArrow->setVisible(false);

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

    //Add Voltage indicator next to extremity bracket
    leftBracket = new QCPItemText(g);
    leftBracket->position->setParentAnchor(bracket->left);
    leftBracket->setTextAlignment(Qt::AlignCenter);
    s_fontUtsaah.setBold(false);
    s_fontUtsaah.setPointSize(9);
    leftBracket->setFont(s_fontUtsaah);
    leftBracket->setColor(s_couleurMeasure);
    leftBracket->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    leftBracket->position->setCoords(0,-20);
    leftBracket->setVisible(false);

    rightBracket = new QCPItemText(g);
    rightBracket->position->setParentAnchor(bracket->right);
    rightBracket->setTextAlignment(Qt::AlignCenter);
    rightBracket->setFont(s_fontUtsaah);
    rightBracket->setColor(s_couleurMeasure);
    rightBracket->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    rightBracket->position->setCoords(0,-20);
    rightBracket->setVisible(false);


    textMeasure = new QCPItemText(g);
    textMeasure->position->setType(QCPItemPosition::ptViewportRatio);
    textMeasure->position->setCoords(0.2,0.9);
    textMeasure->setText("Temps : ?");
    textMeasure->setTextAlignment(Qt::AlignRight);
    s_fontUtsaah.setBold(true);
    s_fontUtsaah.setPointSize(12);
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

    if(_now){
        now();
        showMeasureOutside();
        return;
    }

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
        _mainWindow->_volt->setResolution(0.001);
        _mainWindow->_volt->registerValueCallback(CBVolt);
        m->registerBeaconCallback(CBMotteurBeacon);
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

void MainWindow::CBMotteurBeacon(YModule *m , int i)
{
    Q_UNUSED(m)
    emit _mainWindow->beaconChange((i==1)?true:false);
}

void MainWindow::closeEvent(QCloseEvent*)
{
    run = false;
    if(_moteur != nullptr){
        _moteur->module()->muteValueCallbacks();
        _moteur->module()->setBeacon(Y_BEACON_OFF);
    }
    watch.waitForFinished();
}

MainWindow::~MainWindow()
{
    delete ui;
}
