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

#include "qlibcameracamerasession.h"

#include "libcamera/libcamera.h"
#include "qlibcameravideooutput.h"
#include "qlibcameramediavideoprobecontrol.h"
#include "qlibcameramultimediautils.h"
#include "qlibcameracameravideorenderercontrol.h"

#include "libdrm/drm_fourcc.h"

#include <qabstractvideosurface.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <qfile.h>
#include <qguiapplication.h>
#include <qdebug.h>
#include <qvideoframe.h>
#include <private/qmemoryvideobuffer_p.h>
#include <private/qvideoframe_p.h>

static QLibcameraCameraSession *g_currentCameraSession = nullptr;

QT_BEGIN_NAMESPACE

QLibcameraCameraSession::QLibcameraCameraSession(QObject *parent)
    : QObject(parent)
    , m_selectedCamera(0)
    , m_camera(0)
    , m_nativeOrientation(0)
    , m_videoOutput(0)
    , m_captureMode(QCamera::CaptureStillImage)
    , m_state(QCamera::UnloadedState)
    , m_savedState(-1)
    , m_status(QCamera::UnloadedStatus)
    , m_previewStarted(false)
    , m_captureDestination(QCameraImageCapture::CaptureToFile)
    , m_captureImageDriveMode(QCameraImageCapture::SingleImageCapture)
    , m_lastImageCaptureId(0)
    , m_readyForCapture(false)
    , m_captureCanceled(false)
    , m_currentImageCaptureId(-1)
    , m_previewCallback(0)
{
    /*
    m_mediaStorageLocation.addStorageLocation(
                QMediaStorageLocation::Pictures,
                LibcameraMultimediaUtils::getDefaultMediaDirectory(LibcameraMultimediaUtils::DCIM));
    */
    if (qApp) {
        connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
                this, SLOT(onApplicationStateChanged(Qt::ApplicationState)));
    }

    g_currentCameraSession = this;
}

QLibcameraCameraSession::~QLibcameraCameraSession()
{
    close();

    if(g_currentCameraSession==this) g_currentCameraSession = nullptr;
}

void QLibcameraCameraSession::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_captureMode == mode || !isCaptureModeSupported(mode))
        return;

    m_captureMode = mode;
    emit captureModeChanged(m_captureMode);

    if (m_previewStarted && m_captureMode.testFlag(QCamera::CaptureStillImage))
        applyViewfinderSettings(m_actualImageSettings.resolution());
}

bool QLibcameraCameraSession::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    if (mode & (QCamera::CaptureStillImage & QCamera::CaptureVideo))
        return false;

    return true;
}

void QLibcameraCameraSession::setState(QCamera::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);

    // If the application is inactive, the camera shouldn't be started. Save the desired state
    // instead and it will be set when the application becomes active.
    if (qApp->applicationState() == Qt::ApplicationActive)
        setStateHelper(state);
    else
        m_savedState = state;
}

void QLibcameraCameraSession::setStateHelper(QCamera::State state)
{
    switch (state) {
    case QCamera::UnloadedState:
        close();
        break;
    case QCamera::LoadedState:
    case QCamera::ActiveState:
        if (!m_camera && !open()) {
            m_state = QCamera::UnloadedState;
            emit stateChanged(m_state);
            emit error(QCamera::CameraError, QStringLiteral("Failed to open camera"));
            m_status = QCamera::UnloadedStatus;
            emit statusChanged(m_status);
            return;
        }
        if (state == QCamera::ActiveState)
            startPreview();
        else if (state == QCamera::LoadedState)
            stopPreview();
        break;
    }
}

const std::vector<std::shared_ptr<libcamera::Camera>> &QLibcameraCameraSession::availableCameras()
{
    return g_currentCameraSession->m_cameraManager.cameras();
}

bool QLibcameraCameraSession::open()
{
    close();

    m_status = QCamera::LoadingStatus;
    emit statusChanged(m_status);

    m_camera = m_cameraManager.cameras()[m_selectedCamera];

    if (m_camera) {
        /*
        connect(m_camera, SIGNAL(pictureExposed()), this, SLOT(onCameraPictureExposed()));
        connect(m_camera, SIGNAL(lastPreviewFrameFetched(QVideoFrame)),
                this, SLOT(onLastPreviewFrameFetched(QVideoFrame)),
                Qt::DirectConnection);
        connect(m_camera, SIGNAL(newPreviewFrame(QVideoFrame)),
                this, SLOT(onNewPreviewFrame(QVideoFrame)),
                Qt::DirectConnection);
        connect(m_camera, SIGNAL(pictureCaptured(QByteArray)), this, SLOT(onCameraPictureCaptured(QByteArray)));
        connect(m_camera, SIGNAL(previewStarted()), this, SLOT(onCameraPreviewStarted()));
        connect(m_camera, SIGNAL(previewStopped()), this, SLOT(onCameraPreviewStopped()));
        connect(m_camera, &LibcameraCamera::previewFailedToStart, this, &QLibcameraCameraSession::onCameraPreviewFailedToStart);
        connect(m_camera, &LibcameraCamera::takePictureFailed, this, &QLibcameraCameraSession::onCameraTakePictureFailed);
        */

        // m_nativeOrientation = m_camera.getNativeOrientation();

        m_status = QCamera::LoadedStatus;

        if (m_camera->getPreviewFormat() != LibcameraCamera::NV21)
            m_camera->setPreviewFormat(LibcameraCamera::NV21);

        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);

        emit opened();
        emit statusChanged(m_status);
    }

    return m_camera != 0;
}

void QLibcameraCameraSession::close()
{
    if (!m_camera)
        return;

    stopPreview();

    m_status = QCamera::UnloadingStatus;
    emit statusChanged(m_status);

    m_readyForCapture = false;
    m_currentImageCaptureId = -1;
    m_currentImageCaptureFileName.clear();
    m_actualImageSettings = m_requestedImageSettings;
    m_actualViewfinderSettings = m_requestedViewfinderSettings;

    m_camera->release();
    m_camera = 0;

    m_status = QCamera::UnloadedStatus;
    emit statusChanged(m_status);
}

void QLibcameraCameraSession::setVideoOutput(QLibcameraVideoOutput *output)
{
    if (m_videoOutput) {
        m_videoOutput->stop();
        m_videoOutput->reset();
    }

    if (output) {
        m_videoOutput = output;
        if (m_videoOutput->isReady())
            onVideoOutputReady(true);
        else
            connect(m_videoOutput, SIGNAL(readyChanged(bool)), this, SLOT(onVideoOutputReady(bool)));
    } else {
        m_videoOutput = 0;
    }
}

void QLibcameraCameraSession::setViewfinderSettings(const QCameraViewfinderSettings &settings)
{
    if (m_requestedViewfinderSettings == settings)
        return;

    m_requestedViewfinderSettings = m_actualViewfinderSettings = settings;

    if (m_readyForCapture)
        applyViewfinderSettings();
}

void QLibcameraCameraSession::applyViewfinderSettings(const QSize &captureSize, bool restartPreview)
{
    if (!m_camera)
        return;

    const QSize currentViewfinderResolution = m_camera->previewSize();
    const LibcameraCamera::ImageFormat currentPreviewFormat = m_camera->getPreviewFormat();
    const LibcameraCamera::FpsRange currentFpsRange = m_camera->getPreviewFpsRange();

    // -- adjust resolution
    QSize adjustedViewfinderResolution;
    const bool validCaptureSize = captureSize.width() > 0 && captureSize.height() > 0;
    if (m_captureMode.testFlag(QCamera::CaptureVideo)
            && validCaptureSize
            && m_camera->getPreferredPreviewSizeForVideo().isEmpty()) {
        // According to the Libcamera doc, if getPreferredPreviewSizeForVideo() returns null, it means
        // the preview size cannot be different from the capture size
        adjustedViewfinderResolution = captureSize;
    } else {
        qreal captureAspectRatio = 0;
        if (validCaptureSize)
            captureAspectRatio = qreal(captureSize.width()) / qreal(captureSize.height());

        const QList<QSize> previewSizes = m_camera->getSupportedPreviewSizes();

        const QSize vfRes = m_requestedViewfinderSettings.resolution();
        if (vfRes.width() > 0 && vfRes.height() > 0
                && (!validCaptureSize || qAbs(captureAspectRatio - (qreal(vfRes.width()) / vfRes.height())) < 0.01)
                && previewSizes.contains(vfRes)) {
            adjustedViewfinderResolution = vfRes;
        } else if (validCaptureSize) {
            // search for viewfinder resolution with the same aspect ratio
            qreal minAspectDiff = 1;
            QSize closestResolution;
            for (int i = previewSizes.count() - 1; i >= 0; --i) {
                const QSize &size = previewSizes.at(i);
                const qreal sizeAspect = qreal(size.width()) / size.height();
                if (qFuzzyCompare(captureAspectRatio, sizeAspect)) {
                    adjustedViewfinderResolution = size;
                    break;
                } else if (minAspectDiff > qAbs(sizeAspect - captureAspectRatio)) {
                    closestResolution = size;
                    minAspectDiff = qAbs(sizeAspect - captureAspectRatio);
                }
            }
            if (!adjustedViewfinderResolution.isValid()) {
                qWarning("Cannot find a viewfinder resolution matching the capture aspect ratio.");
                if (closestResolution.isValid()) {
                    adjustedViewfinderResolution = closestResolution;
                    qWarning("Using closest viewfinder resolution.");
                } else {
                    return;
                }
            }
        } else {
            adjustedViewfinderResolution = previewSizes.last();
        }
    }
    m_actualViewfinderSettings.setResolution(adjustedViewfinderResolution);

    // -- adjust pixel format

    LibcameraCamera::ImageFormat adjustedPreviewFormat = LibcameraCamera::NV21;
    if (m_requestedViewfinderSettings.pixelFormat() != QVideoFrame::Format_Invalid) {
        const LibcameraCamera::ImageFormat f = LibcameraImageFormatFromQtPixelFormat(m_requestedViewfinderSettings.pixelFormat());
        if (f == LibcameraCamera::UnknownImageFormat || !m_camera->getSupportedPreviewFormats().contains(f))
            qWarning("Unsupported viewfinder pixel format");
        else
            adjustedPreviewFormat = f;
    }
    m_actualViewfinderSettings.setPixelFormat(QtPixelFormatFromLibcameraImageFormat(adjustedPreviewFormat));

    // -- adjust FPS

    LibcameraCamera::FpsRange adjustedFps = currentFpsRange;
    const LibcameraCamera::FpsRange requestedFpsRange = LibcameraCamera::FpsRange::makeFromQReal(m_requestedViewfinderSettings.minimumFrameRate(),
                                                                                             m_requestedViewfinderSettings.maximumFrameRate());
    if (requestedFpsRange.min > 0 || requestedFpsRange.max > 0) {
        int minDist = INT_MAX;
        const QList<LibcameraCamera::FpsRange> supportedFpsRanges = m_camera->getSupportedPreviewFpsRange();
        auto it = supportedFpsRanges.rbegin(), end = supportedFpsRanges.rend();
        for (; it != end; ++it) {
            int dist = (requestedFpsRange.min > 0 ? qAbs(requestedFpsRange.min - it->min) : 0)
                       + (requestedFpsRange.max > 0 ? qAbs(requestedFpsRange.max - it->max) : 0);
            if (dist < minDist) {
                minDist = dist;
                adjustedFps = *it;
                if (minDist == 0)
                    break; // exact match
            }
        }
    }
    m_actualViewfinderSettings.setMinimumFrameRate(adjustedFps.getMinReal());
    m_actualViewfinderSettings.setMaximumFrameRate(adjustedFps.getMaxReal());

    // -- Set values on camera

    if (currentViewfinderResolution != adjustedViewfinderResolution
            || currentPreviewFormat != adjustedPreviewFormat
            || currentFpsRange.min != adjustedFps.min
            || currentFpsRange.max != adjustedFps.max) {

        if (m_videoOutput)
            m_videoOutput->setVideoSize(adjustedViewfinderResolution);

        // if preview is started, we have to stop it first before changing its size
        if (m_previewStarted && restartPreview)
            m_camera->stopPreview();

        m_camera->setPreviewSize(adjustedViewfinderResolution);
        m_camera->setPreviewFormat(adjustedPreviewFormat);
        m_camera->setPreviewFpsRange(adjustedFps);

        // restart preview
        if (m_previewStarted && restartPreview)
            m_camera->startPreview();
    }
}

QList<QSize> QLibcameraCameraSession::getSupportedPreviewSizes() const
{
    return m_camera ? m_camera->getSupportedPreviewSizes() : QList<QSize>();
}

QList<QVideoFrame::PixelFormat> QLibcameraCameraSession::getSupportedPixelFormats() const
{
    QList<QVideoFrame::PixelFormat> formats;

    if (!m_camera)
        return formats;

    const QList<LibcameraCamera::ImageFormat> nativeFormats = m_camera->getSupportedPreviewFormats();

    formats.reserve(nativeFormats.size());

    for (LibcameraCamera::ImageFormat nativeFormat : nativeFormats) {
        QVideoFrame::PixelFormat format = QtPixelFormatFromLibcameraImageFormat(nativeFormat);
        if (format != QVideoFrame::Format_Invalid)
            formats.append(format);
    }

    return formats;
}

QList<LibcameraCamera::FpsRange> QLibcameraCameraSession::getSupportedPreviewFpsRange() const
{
    return m_camera ? m_camera->getSupportedPreviewFpsRange() : QList<LibcameraCamera::FpsRange>();
}

struct NullSurface : QAbstractVideoSurface
{
    NullSurface(QObject *parent = nullptr) : QAbstractVideoSurface(parent) { }
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override
    {
        QList<QVideoFrame::PixelFormat> result;
        if (type == QAbstractVideoBuffer::NoHandle)
            result << QVideoFrame::Format_NV21;

        return result;
    }

    bool present(const QVideoFrame &)  { return false; }
};

bool QLibcameraCameraSession::startPreview()
{
    if (!m_camera)
        return false;

    if (m_previewStarted)
        return true;

    if (m_videoOutput) {
        if (!m_videoOutput->isReady())
            return true; // delay starting until the video output is ready

        Q_ASSERT(m_videoOutput->surfaceTexture() || m_videoOutput->surfaceHolder());

        if ((m_videoOutput->surfaceTexture() && !m_camera->setPreviewTexture(m_videoOutput->surfaceTexture()))
                || (m_videoOutput->surfaceHolder() && !m_camera->setPreviewDisplay(m_videoOutput->surfaceHolder())))
            return false;
    } else {
        auto control = new QLibcameraCameraVideoRendererControl(this, this);
        control->setSurface(new NullSurface(this));
        qWarning() << "Starting camera without viewfinder available";

        return true;
    }

    m_status = QCamera::StartingStatus;
    emit statusChanged(m_status);

    applyImageSettings();
    applyViewfinderSettings(m_captureMode.testFlag(QCamera::CaptureStillImage) ? m_actualImageSettings.resolution()
                                                                               : QSize());

    LibcameraMultimediaUtils::enableOrientationListener(true);

    // Before API level 24 the orientation was always 0, which is what we're expecting, so
    // we'll enforce that here.
    if (QtLibcameraPrivate::libcameraSdkVersion() > 23)
        m_camera->setDisplayOrientation(0);

    m_camera->startPreview();
    m_previewStarted = true;

    return true;
}

void QLibcameraCameraSession::stopPreview()
{
    if (!m_camera || !m_previewStarted)
        return;

    m_status = QCamera::StoppingStatus;
    emit statusChanged(m_status);

    LibcameraMultimediaUtils::enableOrientationListener(false);

    m_camera->stopPreview();
    m_camera->setPreviewSize(QSize());
    m_camera->setPreviewTexture(0);
    m_camera->setPreviewDisplay(0);

    if (m_videoOutput) {
        m_videoOutput->stop();
        m_videoOutput->reset();
    }
    m_previewStarted = false;
}

void QLibcameraCameraSession::setImageSettings(const QImageEncoderSettings &settings)
{
    if (m_requestedImageSettings == settings)
        return;

    m_requestedImageSettings = m_actualImageSettings = settings;

    applyImageSettings();

    if (m_readyForCapture && m_captureMode.testFlag(QCamera::CaptureStillImage))
        applyViewfinderSettings(m_actualImageSettings.resolution());
}

int QLibcameraCameraSession::currentCameraRotation() const
{
    if (!m_camera)
        return 0;

    // subtract natural camera orientation and physical device orientation
    int rotation = 0;
    int deviceOrientation = (LibcameraMultimediaUtils::getDeviceOrientation() + 45) / 90 * 90;
    if (m_camera->getFacing() == LibcameraCamera::CameraFacingFront)
        rotation = (m_nativeOrientation - deviceOrientation + 360) % 360;
    else // back-facing camera
        rotation = (m_nativeOrientation + deviceOrientation) % 360;

    return rotation;
}

void QLibcameraCameraSession::addProbe(QLibcameraMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    if (probe)
        m_videoProbes << probe;
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::removeProbe(QLibcameraMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    m_videoProbes.remove(probe);
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::setPreviewFormat(LibcameraCamera::ImageFormat format)
{
    if (format == LibcameraCamera::UnknownImageFormat)
        return;

    m_camera->setPreviewFormat(format);
}

void QLibcameraCameraSession::setPreviewCallback(PreviewCallback *callback)
{
    m_videoProbesMutex.lock();
    m_previewCallback = callback;
    if (m_camera)
        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::applyImageSettings()
{
    if (!m_camera)
        return;

    if (m_actualImageSettings.codec().isEmpty())
        m_actualImageSettings.setCodec(QLatin1String("jpeg"));

    const QSize requestedResolution = m_requestedImageSettings.resolution();
    const QList<QSize> supportedResolutions = m_camera->getSupportedPictureSizes();
    if (!requestedResolution.isValid()) {
        // if the viewfinder resolution is explicitly set, pick the highest available capture
        // resolution with the same aspect ratio
        if (m_requestedViewfinderSettings.resolution().isValid()) {
            const QSize vfResolution = m_actualViewfinderSettings.resolution();
            const qreal vfAspectRatio = qreal(vfResolution.width()) / vfResolution.height();

            auto it = supportedResolutions.rbegin(), end = supportedResolutions.rend();
            for (; it != end; ++it) {
                if (qAbs(vfAspectRatio - (qreal(it->width()) / it->height())) < 0.01) {
                    m_actualImageSettings.setResolution(*it);
                    break;
                }
            }
        } else {
            // otherwise, use the highest supported one
            m_actualImageSettings.setResolution(supportedResolutions.last());
        }
    } else if (!supportedResolutions.contains(requestedResolution)) {
        // if the requested resolution is not supported, find the closest one
        int reqPixelCount = requestedResolution.width() * requestedResolution.height();
        QList<int> supportedPixelCounts;
        for (int i = 0; i < supportedResolutions.size(); ++i) {
            const QSize &s = supportedResolutions.at(i);
            supportedPixelCounts.append(s.width() * s.height());
        }
        int closestIndex = qt_findClosestValue(supportedPixelCounts, reqPixelCount);
        m_actualImageSettings.setResolution(supportedResolutions.at(closestIndex));
    }
    m_camera->setPictureSize(m_actualImageSettings.resolution());

    int jpegQuality = 100;
    switch (m_requestedImageSettings.quality()) {
    case QMultimedia::VeryLowQuality:
        jpegQuality = 20;
        break;
    case QMultimedia::LowQuality:
        jpegQuality = 40;
        break;
    case QMultimedia::NormalQuality:
        jpegQuality = 60;
        break;
    case QMultimedia::HighQuality:
        jpegQuality = 80;
        break;
    case QMultimedia::VeryHighQuality:
        jpegQuality = 100;
        break;
    }
    m_camera->setJpegQuality(jpegQuality);
}

bool QLibcameraCameraSession::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
{
    return destination & (QCameraImageCapture::CaptureToFile | QCameraImageCapture::CaptureToBuffer);
}

QCameraImageCapture::CaptureDestinations QLibcameraCameraSession::captureDestination() const
{
    return m_captureDestination;
}

void QLibcameraCameraSession::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
{
    if (m_captureDestination != destination) {
        m_captureDestination = destination;
        emit captureDestinationChanged(m_captureDestination);
    }
}

bool QLibcameraCameraSession::isReadyForCapture() const
{
    return m_status == QCamera::ActiveStatus && m_readyForCapture;
}

void QLibcameraCameraSession::setReadyForCapture(bool ready)
{
    if (m_readyForCapture == ready)
        return;

    m_readyForCapture = ready;
    emit readyForCaptureChanged(ready);
}

QCameraImageCapture::DriveMode QLibcameraCameraSession::driveMode() const
{
    return m_captureImageDriveMode;
}

void QLibcameraCameraSession::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    m_captureImageDriveMode = mode;
}

int QLibcameraCameraSession::capture(const QString &fileName)
{
    ++m_lastImageCaptureId;

    if (!isReadyForCapture()) {
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotReadyError,
                               tr("Camera not ready"));
        return m_lastImageCaptureId;
    }

    if (m_captureImageDriveMode == QCameraImageCapture::SingleImageCapture) {
        setReadyForCapture(false);

        m_currentImageCaptureId = m_lastImageCaptureId;
        m_currentImageCaptureFileName = fileName;

        applyImageSettings();
        applyViewfinderSettings(m_actualImageSettings.resolution());

        // adjust picture rotation depending on the device orientation
        m_camera->setRotation(currentCameraRotation());

        m_camera->takePicture();
    } else {
        //: Drive mode is the camera's shutter mode, for example single shot, continuos exposure, etc.
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotSupportedFeatureError,
                               tr("Drive mode not supported"));
    }

    return m_lastImageCaptureId;
}

void QLibcameraCameraSession::cancelCapture()
{
    if (m_readyForCapture)
        return;

    m_captureCanceled = true;
}

void QLibcameraCameraSession::onCameraTakePictureFailed()
{
    emit imageCaptureError(m_currentImageCaptureId, QCameraImageCapture::ResourceError,
                           tr("Failed to capture image"));

    // Preview needs to be restarted and the preview call back must be setup again
    m_camera->startPreview();
}

void QLibcameraCameraSession::onCameraPictureExposed()
{
    if (m_captureCanceled || !m_camera)
        return;

    emit imageExposed(m_currentImageCaptureId);
    m_camera->fetchLastPreviewFrame();
}

void QLibcameraCameraSession::onLastPreviewFrameFetched(const QVideoFrame &frame)
{
    if (m_captureCanceled || !m_camera)
        return;

    QtConcurrent::run(this, &QLibcameraCameraSession::processPreviewImage,
                      m_currentImageCaptureId,
                      frame,
                      m_camera->getRotation());
}

void QLibcameraCameraSession::processPreviewImage(int id, const QVideoFrame &frame, int rotation)
{
    // Preview display of front-facing cameras is flipped horizontally, but the frame data
    // we get here is not. Flip it ourselves if the camera is front-facing to match what the user
    // sees on the viewfinder.
    QTransform transform;
    if (m_camera->getFacing() == LibcameraCamera::CameraFacingFront)
        transform.scale(-1, 1);
    transform.rotate(rotation);

    emit imageCaptured(id, qt_imageFromVideoFrame(frame).transformed(transform));
}

void QLibcameraCameraSession::onNewPreviewFrame(const QVideoFrame &frame)
{
    if (!m_camera)
        return;

    m_videoProbesMutex.lock();

    for (QLibcameraMediaVideoProbeControl *probe : qAsConst(m_videoProbes))
        probe->newFrameProbed(frame);

    if (m_previewCallback)
        m_previewCallback->onFrameAvailable(frame);

    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::onCameraPictureCaptured(const QByteArray &data)
{
    if (!m_captureCanceled) {
        // Loading and saving the captured image can be slow, do it in a separate thread
        QtConcurrent::run(this, &QLibcameraCameraSession::processCapturedImage,
                          m_currentImageCaptureId,
                          data,
                          m_actualImageSettings.resolution(),
                          m_captureDestination,
                          m_currentImageCaptureFileName);
    }

    m_captureCanceled = false;

    // Preview needs to be restarted after taking a picture
    if (m_camera)
        m_camera->startPreview();
}

void QLibcameraCameraSession::onCameraPreviewStarted()
{
    if (m_status == QCamera::StartingStatus) {
        m_status = QCamera::ActiveStatus;
        emit statusChanged(m_status);
    }

    setReadyForCapture(true);
}

void QLibcameraCameraSession::onCameraPreviewFailedToStart()
{
    if (m_status == QCamera::StartingStatus) {
        Q_EMIT error(QCamera::CameraError, tr("Camera preview failed to start."));

        LibcameraMultimediaUtils::enableOrientationListener(false);
        m_camera->setPreviewSize(QSize());
        m_camera->setPreviewTexture(0);
        if (m_videoOutput) {
            m_videoOutput->stop();
            m_videoOutput->reset();
        }
        m_previewStarted = false;

        m_status = QCamera::LoadedStatus;
        emit statusChanged(m_status);

        setReadyForCapture(false);
    }
}

void QLibcameraCameraSession::onCameraPreviewStopped()
{
    if (m_status == QCamera::StoppingStatus) {
        m_status = QCamera::LoadedStatus;
        emit statusChanged(m_status);
    }

    setReadyForCapture(false);
}

void QLibcameraCameraSession::processCapturedImage(int id,
                                                 const QByteArray &data,
                                                 const QSize &resolution,
                                                 QCameraImageCapture::CaptureDestinations dest,
                                                 const QString &fileName)
{


    if (dest & QCameraImageCapture::CaptureToFile) {
        const QString actualFileName = m_mediaStorageLocation.generateFileName(fileName,
                                                                               QMediaStorageLocation::Pictures,
                                                                               QLatin1String("IMG_"),
                                                                               QLatin1String("jpg"));

        QFile file(actualFileName);
        if (file.open(QFile::WriteOnly)) {
            if (file.write(data) == data.size()) {
                // if the picture is saved into the standard picture location, register it
                // with the Libcamera media scanner so it appears immediately in apps
                // such as the gallery.
                QString standardLoc = LibcameraMultimediaUtils::getDefaultMediaDirectory(LibcameraMultimediaUtils::DCIM);
                if (actualFileName.startsWith(standardLoc))
                    LibcameraMultimediaUtils::registerMediaFile(actualFileName);

                emit imageSaved(id, actualFileName);
            } else {
                emit imageCaptureError(id, QCameraImageCapture::OutOfSpaceError, file.errorString());
            }
        } else {
            const QString errorMessage = tr("Could not open destination file: %1").arg(actualFileName);
            emit imageCaptureError(id, QCameraImageCapture::ResourceError, errorMessage);
        }
    }

    if (dest & QCameraImageCapture::CaptureToBuffer) {
        QVideoFrame frame(new QMemoryVideoBuffer(data, -1), resolution, QVideoFrame::Format_Jpeg);
        emit imageAvailable(id, frame);
    }
}

QVideoFrame::PixelFormat QLibcameraCameraSession::QtPixelFormatFromLibcameraImageFormat(LibcameraCamera::ImageFormat format)
{
    switch (format) {
    case LibcameraCamera::RGB565:
        return QVideoFrame::Format_RGB565;
    case LibcameraCamera::NV21:
        return QVideoFrame::Format_NV21;
    case LibcameraCamera::YUY2:
        return QVideoFrame::Format_YUYV;
    case LibcameraCamera::JPEG:
        return QVideoFrame::Format_Jpeg;
    case LibcameraCamera::YV12:
        return QVideoFrame::Format_YV12;
    default:
        return QVideoFrame::Format_Invalid;
    }
}

LibcameraCamera::ImageFormat QLibcameraCameraSession::LibcameraImageFormatFromQtPixelFormat(QVideoFrame::PixelFormat format)
{
    switch (format) {
    case QVideoFrame::Format_RGB565:
        return LibcameraCamera::RGB565;
    case QVideoFrame::Format_NV21:
        return LibcameraCamera::NV21;
    case QVideoFrame::Format_YUYV:
        return LibcameraCamera::YUY2;
    case QVideoFrame::Format_Jpeg:
        return LibcameraCamera::JPEG;
    case QVideoFrame::Format_YV12:
        return LibcameraCamera::YV12;
    default:
        return LibcameraCamera::UnknownImageFormat;
    }
}

void QLibcameraCameraSession::onVideoOutputReady(bool ready)
{
    if (ready && m_state == QCamera::ActiveState)
        startPreview();
}

void QLibcameraCameraSession::onApplicationStateChanged(Qt::ApplicationState state)
{
    switch (state) {
    case Qt::ApplicationInactive:
        if (m_state != QCamera::UnloadedState) {
            m_savedState = m_state;
            close();
            m_state = QCamera::UnloadedState;
            emit stateChanged(m_state);
        }
        break;
    case Qt::ApplicationActive:
        if (m_savedState != -1) {
            setStateHelper(QCamera::State(m_savedState));
            m_savedState = -1;
        }
        break;
    default:
        break;
    }
}

QT_END_NAMESPACE
