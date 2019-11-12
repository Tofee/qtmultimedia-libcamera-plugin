/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlibcameramediaserviceplugin.h"

#include "mediacapture/qlibcameracaptureservice.h"
#include "mediacapture/qlibcameracapturesession.h"
#include "mediacapture/qlibcameracamerasession.h"
#include "mediacapture/qlibcameracamerainfocontrol.h"

#include "common/qlibcameraglobal.h"

#include "libcamera/libcamera.h"

Q_LOGGING_CATEGORY(qtLibcameraMediaPlugin, "qt.multimedia.plugins.libcamera")

QLibcameraMediaServicePlugin::QLibcameraMediaServicePlugin()
{
}

QLibcameraMediaServicePlugin::~QLibcameraMediaServicePlugin()
{
}

QMediaService *QLibcameraMediaServicePlugin::create(const QString &key)
{
    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        return new QLibcameraCaptureService(key);
    }

    qCWarning(qtLibcameraMediaPlugin) << "Libcamera service plugin: unsupported key:" << key;
    return 0;
}

void QLibcameraMediaServicePlugin::release(QMediaService *service)
{
    delete service;
}

QMediaServiceProviderHint::Features QLibcameraMediaServicePlugin::supportedFeatures(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QMediaServiceProviderHint::VideoSurface | QMediaServiceProviderHint::RecordingSupport;

    return QMediaServiceProviderHint::Features();
}

QByteArray QLibcameraMediaServicePlugin::defaultDevice(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA && !QLibcameraCameraSession::availableCameras().empty())
        return QLibcameraCameraSession::availableCameras().at(0)->name().c_str();

    return QByteArray();
}

QList<QByteArray> QLibcameraMediaServicePlugin::devices(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        QList<QByteArray> devices;
        for (const auto &camera: QLibcameraCameraSession::availableCameras())
            devices.append(camera->name().c_str());
        return devices;
    }

    return QList<QByteArray>();
}

QString QLibcameraMediaServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        for (const auto &camera: QLibcameraCameraSession::availableCameras())
            if (device == camera->name().c_str())
                return camera->name().c_str();
    }

    return QString();
}

QCamera::Position QLibcameraMediaServicePlugin::cameraPosition(const QByteArray &device) const
{
    return QLibcameraCameraInfoControl::position(device);
}

int QLibcameraMediaServicePlugin::cameraOrientation(const QByteArray &device) const
{
    return QLibcameraCameraInfoControl::orientation(device);
}

