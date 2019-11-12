/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Ruslan Baratov
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

#ifndef QLIBCAMERACAPTURESERVICE_H
#define QLIBCAMERACAPTURESERVICE_H

#include <qmediaservice.h>
#include <qmediacontrol.h>

QT_BEGIN_NAMESPACE

class QLibcameraMediaRecorderControl;
class QLibcameraCaptureSession;
class QLibcameraCameraControl;
class QLibcameraCameraInfoControl;
class QLibcameraVideoDeviceSelectorControl;
class QLibcameraAudioInputSelectorControl;
class QLibcameraCameraSession;
class QLibcameraCameraVideoRendererControl;
class QLibcameraCameraZoomControl;
class QLibcameraCameraExposureControl;
class QLibcameraCameraFlashControl;
class QLibcameraCameraFocusControl;
class QLibcameraViewfinderSettingsControl2;
class QLibcameraCameraLocksControl;
class QLibcameraCameraImageProcessingControl;
class QLibcameraImageEncoderControl;
class QLibcameraCameraImageCaptureControl;
class QLibcameraCameraCaptureDestinationControl;
class QLibcameraCameraCaptureBufferFormatControl;
class QLibcameraAudioEncoderSettingsControl;
class QLibcameraVideoEncoderSettingsControl;
class QLibcameraMediaContainerControl;

class QLibcameraCaptureService : public QMediaService
{
    Q_OBJECT

public:
    explicit QLibcameraCaptureService(const QString &service, QObject *parent = 0);
    virtual ~QLibcameraCaptureService();

    QMediaControl *requestControl(const char *name);
    void releaseControl(QMediaControl *);

private:
    QString m_service;

    QLibcameraMediaRecorderControl *m_recorderControl;
    QLibcameraCaptureSession *m_captureSession;
    QLibcameraCameraControl *m_cameraControl;
    QLibcameraCameraInfoControl *m_cameraInfoControl;
    QLibcameraVideoDeviceSelectorControl *m_videoInputControl;
    QLibcameraAudioInputSelectorControl *m_audioInputControl;
    QLibcameraCameraSession *m_cameraSession;
    QLibcameraCameraVideoRendererControl *m_videoRendererControl;
    QLibcameraCameraZoomControl *m_cameraZoomControl;
    QLibcameraCameraExposureControl *m_cameraExposureControl;
    QLibcameraCameraFlashControl *m_cameraFlashControl;
    QLibcameraCameraFocusControl *m_cameraFocusControl;
    QLibcameraViewfinderSettingsControl2 *m_viewfinderSettingsControl2;
    QLibcameraCameraLocksControl *m_cameraLocksControl;
    QLibcameraCameraImageProcessingControl *m_cameraImageProcessingControl;
    QLibcameraImageEncoderControl *m_imageEncoderControl;
    QLibcameraCameraImageCaptureControl *m_imageCaptureControl;
    QLibcameraCameraCaptureDestinationControl *m_captureDestinationControl;
    QLibcameraCameraCaptureBufferFormatControl *m_captureBufferFormatControl;
    QLibcameraAudioEncoderSettingsControl *m_audioEncoderSettingsControl;
    QLibcameraVideoEncoderSettingsControl *m_videoEncoderSettingsControl;
    QLibcameraMediaContainerControl *m_mediaContainerControl;
};

QT_END_NAMESPACE

#endif // QLIBCAMERACAPTURESERVICE_H
