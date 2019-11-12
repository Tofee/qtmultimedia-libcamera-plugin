TARGET = qtmedia_libcamera

QT += multimedia-private core-private network

CONFIG += link_pkgconfig
PKGCONFIG += camera
INCLUDEPATH += /usr/include/libcamera

HEADERS += \
    qlibcameramediaserviceplugin.h

SOURCES += \
    qlibcameramediaserviceplugin.cpp

include (common/common.pri)
# include (mediaplayer/mediaplayer.pri)
include (mediacapture/mediacapture.pri)

OTHER_FILES += libcamera_mediaservice.json

PLUGIN_TYPE = mediaservice
PLUGIN_CLASS_NAME = QLibcameraMediaServicePlugin
load(qt_plugin)
