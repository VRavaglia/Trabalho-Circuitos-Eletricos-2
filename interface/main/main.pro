TEMPLATE = app
TARGET = main

QT = core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    mna1.cpp

HEADERS += \
    mainwindow.h \
    mna1.h

FORMS += \
    mainwindow.ui
