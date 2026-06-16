QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
TEMPLATE = app
TARGET = ExceptionDumpAnalyzer

DEFINES += QT_DEPRECATED_WARNINGS

QMAKE_CXXFLAGS += -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8

BUILD_ROOT = $$PWD/packages/build-$${TARGET}-qmake
CONFIG(debug, debug|release) {
    BUILD_CONFIG = Debug
    BUILD_OUTPUT = debug
} else {
    BUILD_CONFIG = Release
    BUILD_OUTPUT = release
}

BUILD_DIR = $$BUILD_ROOT-$$BUILD_CONFIG
DESTDIR = $$BUILD_DIR/$$BUILD_OUTPUT
OBJECTS_DIR = $$BUILD_DIR/obj
MOC_DIR = $$BUILD_DIR/moc
RCC_DIR = $$BUILD_DIR/rcc
UI_DIR = $$BUILD_DIR/ui

INCLUDEPATH += $$PWD/include \
               $$PWD/include/core \
               $$PWD/include/ui \
               $$PWD/include/can \
               $$PWD/lib/ECanVci \
               $$PWD/lib/zlg

SOURCES += \
    src/main.cpp \
    src/ui/main_window.cpp \
    src/can/can_driver.cpp \
    src/can/can_transfer_widget.cpp \
    src/can/can_worker.cpp \
    src/can/ecan_vci_driver.cpp \
    src/can/zlg_can_driver.cpp \
    src/core/exception_record.cpp \
    src/core/symbol_resolver.cpp

HEADERS += \
    include/ui/main_window.h \
    include/can/can_driver.h \
    include/can/can_transfer_widget.h \
    include/can/can_worker.h \
    include/can/ecan_vci_driver.h \
    include/can/zlg_can_driver.h \
    include/core/exception_record.h \
    include/core/symbol_resolver.h \
    lib/ECanVci/ECanVci.h \
    lib/zlg/canframe.h \
    lib/zlg/config.h \
    lib/zlg/typedef.h \
    lib/zlg/zlgcan.h

RESOURCES += \
    resources/resources.qrc

win32 {
    RC_ICONS = resources/icons/app_icon.ico
}
