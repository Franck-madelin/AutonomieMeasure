#-------------------------------------------------
#
# Project created by QtCreator 2019-04-04T11:53:55
#
#-------------------------------------------------

QT       += core gui printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(mingw){
    COMPILO = mingw
}
else:CONFIG(msvc){
    COMPILO = msvc
}

TARGET = AutonomieMeasure
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp

HEADERS += \
        mainwindow.h

FORMS += \
        mainwindow.ui


#cusomplot
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/customplot/ -lqcustomplot2
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/customplot/ -lqcustomplotd2
INCLUDEPATH += $$PWD/customplot
DEPENDPATH += $$PWD/customplot

#Yoctopuce
CONFIG(release, debug|release){
    YOCTO =  YoctopuceQtLib-$$COMPILO
}
else:CONFIG(debug, debug|release){
    YOCTO = YoctopuceQtLib-$${COMPILO}_d
}
LIBS += -L$$PWD/yocto -l$$YOCTO

#librairies externes
INCLUDEPATH += $$PWD/yocto/
DEPENDPATH  += $$PWD/yocto/


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
