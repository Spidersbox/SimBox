TARGET = Simbox
TEMPLATE = app
QT += core gui multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

SOURCES += \
    src/Simbox.cpp \
    src/editform.cpp \
    src/SimboxMain.cpp

HEADERS += \
    src/editform.h \
    src/SimboxMain.h

FORMS += \
    src/forms/editform.ui

RC_FILE = src/Simbox.rc

RESOURCES += \
    src/Simbox.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
