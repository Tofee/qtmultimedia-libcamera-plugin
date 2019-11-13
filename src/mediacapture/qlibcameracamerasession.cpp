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
#include "libcameraqteventdispatcher.h"

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

static libcamera::CameraManager *g_cameraManager = nullptr;

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

    std::unique_ptr<libcamera::EventDispatcher> dispatcher(new LibcameraQtEventDispatcher());
    m_cameraManager.setEventDispatcher(std::move(dispatcher));
    m_cameraManager.start();

    g_cameraManager = &m_cameraManager;
}

QLibcameraCameraSession::~QLibcameraCameraSession()
{
    close();

    if(g_cameraManager == &m_cameraManager) g_cameraManager = nullptr;
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
    return g_cameraManager->cameras();
}

bool QLibcameraCameraSession::open()
{
    close();

    m_status = QCamera::LoadingStatus;
    emit statusChanged(m_status);

    m_camera = m_cameraManager.cameras()[m_selectedCamera];

    if (m_camera) {
        m_camera->acquire();

        m_camera->requestCompleted.connect(this, &QLibcameraCameraSession::requestComplete);

        // generate a default Viewfinder configuration
        m_cameraViewfinderConfig = m_camera->generateConfiguration({ StreamRole::Viewfinder });
        m_cameraImageCaptureConfig = m_camera->generateConfiguration({ StreamRole::StillCapture });

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
        /*
        if (m_camera->getPreviewFormat() != LibcameraCamera::NV21)
            m_camera->setPreviewFormat(LibcameraCamera::NV21);

        m_camera->notifyNewFrames(m_videoProbes.count() || m_previewCallback);
        */
        emit opened();
        emit statusChanged(m_status);
    }

    return m_camera != nullptr;
}

void QLibcameraCameraSession::requestComplete(libcamera::Request *request, const std::map<libcamera::Stream *, libcamera::Buffer *> &buffers)
{
    if (request->status() == libcamera::Request::RequestCancelled)
        return;

    libcamera::Buffer *buffer = buffers.begin()->second;
    /// TODO: display(buffer);

    request = m_camera->createRequest();
    if (!request) {
        qCritical("Can't create request");
        return;
    }

    for (auto it = buffers.begin(); it != buffers.end(); ++it) {
        libcamera::Stream *stream = it->first;
        libcamera::Buffer *buffer = it->second;
        unsigned int index = buffer->index();

        std::unique_ptr<libcamera::Buffer> newBuffer = stream->createBuffer(index);
        if (!newBuffer) {
            qCritical() << "Can't create buffer " << index;
            return;
        }

        request->addBuffer(std::move(newBuffer));
    }

    m_camera->queueRequest(request);
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
    m_camera = nullptr;

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
        m_videoOutput = nullptr;
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

    libcamera::StreamConfiguration &cfg = m_cameraViewfinderConfig->at(0);

    QSize sizeToApply = captureSize;
    if(captureSize.isEmpty()){
        sizeToApply = m_requestedViewfinderSettings.resolution();
    }
    if(!sizeToApply.isEmpty())
    {
        cfg.size.width = sizeToApply.width();
        cfg.size.height = sizeToApply.height();
    }

    // -- adjust resolution
    CameraConfiguration::Status validation = m_cameraViewfinderConfig->validate();
    if (validation == CameraConfiguration::Invalid) {
            qWarning("Cannot find a viewfinder resolution matching the capture aspect ratio.");
            return;
    }
    if (validation == CameraConfiguration::Adjusted) {
            qWarning() << "Using closest viewfinder resolution: " << cfg.size.toString().c_str();
    }

    // -- adjust pixel format
    libcamera::PixelFormat adjustedPreviewFormat = DRM_FORMAT_NV21;
    if (m_requestedViewfinderSettings.pixelFormat() != QVideoFrame::Format_Invalid) {
        adjustedPreviewFormat = LibcameraPixelFormatFromQtPixelFormat(m_requestedViewfinderSettings.pixelFormat());
        if (adjustedPreviewFormat == DRM_FORMAT_INVALID)
            qWarning("Unsupported viewfinder pixel format");
        else
        {
            cfg.pixelFormat = adjustedPreviewFormat;
            if(CameraConfiguration::Invalid == m_cameraViewfinderConfig->validate())
                qWarning("Unsupported viewfinder pixel format");
        }
    }

    // -- Set values on camera
    m_camera->configure(m_cameraViewfinderConfig.get());

    /*
    // restart preview
    if (m_previewStarted && restartPreview)
        m_camera->startPreview();
    */
}

QList<QSize> QLibcameraCameraSession::getSupportedPreviewSizes() const
{
    QList<QSize> sizes;

    if(m_cameraViewfinderConfig) {
        libcamera::StreamConfiguration &cfg = m_cameraViewfinderConfig->at(0);
        const libcamera::StreamFormats &nativeFormats = cfg.formats();
        for(const auto &pixelFormat: nativeFormats.pixelformats())
        {
            for(const libcamera::Size &pixelFormatSize: nativeFormats.sizes(pixelFormat))
            {
                QSize qtSize(pixelFormatSize.width, pixelFormatSize.height);
                if(!sizes.contains(qtSize)) sizes.append(qtSize);
            }
        }
    }

    return sizes;
}

QList<QVideoFrame::PixelFormat> QLibcameraCameraSession::getSupportedPixelFormats() const
{
    QList<QVideoFrame::PixelFormat> formats;

    if(m_cameraViewfinderConfig) {
        libcamera::StreamConfiguration &cfg = m_cameraViewfinderConfig->at(0);
        const libcamera::StreamFormats &nativeFormats = cfg.formats();
        for(const auto &nativeFormat: nativeFormats.pixelformats())
        {
            QVideoFrame::PixelFormat format = QtPixelFormatFromLibcameraPixelFormat(nativeFormat);
            if (format != QVideoFrame::Format_Invalid)
                formats.append(format);
        }
    }

    return formats;
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

        //if ((m_videoOutput->surfaceTexture() && !m_camera->setPreviewTexture(m_videoOutput->surfaceTexture()))
        //        || (m_videoOutput->surfaceHolder() && !m_camera->setPreviewDisplay(m_videoOutput->surfaceHolder())))
        //    return false;
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

    // LibcameraMultimediaUtils::enableOrientationListener(true);

    if (m_camera->allocateBuffers()) {
        qCritical("Failed to allocate buffers");
        return false;
    }

    bool success = true;

    libcamera::StreamConfiguration &cfg = m_cameraViewfinderConfig->at(0);
    libcamera::Stream *stream = cfg.stream();
    std::vector<libcamera::Request *> requests;
    for (unsigned int i = 0; i < cfg.bufferCount && success; ++i) {
        libcamera::Request *request = m_camera->createRequest();
        if (!request) {
            qCritical("Can't create request");
            success = false;
            break;
        }

        std::unique_ptr<libcamera::Buffer> buffer = stream->createBuffer(i);
        if (!buffer) {
            qCritical() << "Can't create buffer " << i;
            success = false;
            break;
        }

        if (request->addBuffer(std::move(buffer)) < 0) {
            qCritical("Can't set buffer for request");
            success = false;
            break;
        }

        requests.push_back(request);
    }

    if (success && m_camera->start()) {
        qCritical("Failed to start capture");
        success = false;
    }
    else {
        m_previewStarted = true;

        for (Request *request : requests) {
            if (m_camera->queueRequest(request) < 0) {
                qCritical("Can't queue request");
                success = false;
                break;
            }
        }
    }

    return success;
}

void QLibcameraCameraSession::stopPreview()
{
    if (!m_camera || !m_previewStarted)
        return;

    m_status = QCamera::StoppingStatus;
    emit statusChanged(m_status);

    m_camera->stop();

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
    /// NOT AVAILABLE
    return 0;
}

void QLibcameraCameraSession::addProbe(QLibcameraMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    if (probe)
        m_videoProbes << probe;

    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::removeProbe(QLibcameraMediaVideoProbeControl *probe)
{
    m_videoProbesMutex.lock();
    m_videoProbes.remove(probe);

    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::setPreviewFormat(libcamera::PixelFormat format)
{
    if (format == DRM_FORMAT_INVALID)
        return;

    libcamera::StreamConfiguration &cfg = m_cameraViewfinderConfig->at(0);
    cfg.pixelFormat = format;
    if(CameraConfiguration::Invalid == m_cameraViewfinderConfig->validate())
        qWarning("Unsupported viewfinder pixel format");

    m_camera->configure(m_cameraViewfinderConfig.get());
}

void QLibcameraCameraSession::setPreviewCallback(PreviewCallback *callback)
{
    m_videoProbesMutex.lock();
    m_previewCallback = callback;
    m_videoProbesMutex.unlock();
}

void QLibcameraCameraSession::applyImageSettings()
{
    if (!m_camera)
        return;

    if (m_actualImageSettings.codec().isEmpty())
        m_actualImageSettings.setCodec(QLatin1String("jpeg"));

    /// TODO: setup a nerw stream for image capture, set it up and request a new buffer
    libcamera::StreamConfiguration &cfg = m_cameraImageCaptureConfig->at(0);

    QSize sizeToApply = m_requestedImageSettings.resolution();
    if(!sizeToApply.isEmpty())
    {
        cfg.size.width = sizeToApply.width();
        cfg.size.height = sizeToApply.height();
    }

    // -- adjust resolution
    CameraConfiguration::Status validation = m_cameraImageCaptureConfig->validate();
    if (validation == CameraConfiguration::Invalid) {
            qWarning("Cannot find a viewfinder resolution matching the capture aspect ratio.");
            return;
    }
    if (validation == CameraConfiguration::Adjusted) {
            qWarning() << "Using closest viewfinder resolution: " << cfg.size.toString().c_str();
    }

    // -- Set values on camera ... stop the preview stream ? no ?
    // m_camera->configure(m_cameraImageCaptureConfig.get());

    /*
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
    */
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
        //m_camera->setRotation(currentCameraRotation());

        /// TODO m_camera->takePicture();
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
//    m_camera->startPreview();
}

void QLibcameraCameraSession::onCameraPictureExposed()
{
    if (m_captureCanceled || !m_camera)
        return;

    emit imageExposed(m_currentImageCaptureId);
//    m_camera->fetchLastPreviewFrame();
}

void QLibcameraCameraSession::onLastPreviewFrameFetched(const QVideoFrame &frame)
{
    if (m_captureCanceled || !m_camera)
        return;

    QtConcurrent::run(this, &QLibcameraCameraSession::processPreviewImage,
                      m_currentImageCaptureId,
                      frame,
                      0);
}

void QLibcameraCameraSession::processPreviewImage(int id, const QVideoFrame &frame, int rotation)
{
    // Preview display of front-facing cameras is flipped horizontally, but the frame data
    // we get here is not. Flip it ourselves if the camera is front-facing to match what the user
    // sees on the viewfinder.

//    QTransform transform;
//    if (m_camera->getFacing() == LibcameraCamera::CameraFacingFront)
//        transform.scale(-1, 1);
//    transform.rotate(rotation);

    emit imageCaptured(id, qt_imageFromVideoFrame(frame) /* .transformed(transform) */ );
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
