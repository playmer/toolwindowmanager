#-------------------------------------------------
#
# Project created by QtCreator 2014-03-07T22:02:13
#
#-------------------------------------------------

QT       += core gui widgets

lessThan(QT_MAJOR_VERSION, 5): error("requires Qt 5; found $$[QT_VERSION]")

TARGET = ToolWindowManager
TEMPLATE = app


# Disable conversions to/from const char * in QString
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

INCLUDEPATH += ../src

SOURCES += main.cpp\
  MainWindow.cpp \
  ../src/ToolWindowManager.cpp \
    ../src/ToolWindowManagerArea.cpp \
    ../src/ToolWindowManagerSplitter.cpp \
    ../src/ToolWindowManagerTabBar.cpp \
    ../src/ToolWindowManagerWrapper.cpp

HEADERS  += MainWindow.h \
  ../src/ToolWindowManager.h \
    ../src/ToolWindowManagerArea.h \
    ../src/ToolWindowManagerSplitter.h \
    ../src/ToolWindowManagerTabBar.h \
    ../src/ToolWindowManagerWrapper.h

FORMS    += MainWindow.ui
