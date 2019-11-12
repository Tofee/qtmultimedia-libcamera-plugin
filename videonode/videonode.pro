TARGET = qtsgvideonode_libcamera

QT += quick multimedia-private qtmultimediaquicktools-private

HEADERS += \
    qlibcamerasgvideonodeplugin.h \
    qlibcamerasgvideonode.h

SOURCES += \
    qlibcamerasgvideonodeplugin.cpp \
    qlibcamerasgvideonode.cpp

OTHER_FILES += libcamera_videonode.json

PLUGIN_TYPE = video/videonode
PLUGIN_EXTENDS = quick
PLUGIN_CLASS_NAME = QLibcameraSGVideoNodeFactoryPlugin
load(qt_plugin)
