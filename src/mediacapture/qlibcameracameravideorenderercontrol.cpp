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

#include "qlibcameracameravideorenderercontrol.h"

#include "qlibcameracamerasession.h"
#include "qlibcameravideooutput.h"
#include "libcamerasurfaceview.h"
#include "qlibcameramultimediautils.h"
#include <qabstractvideosurface.h>
#include <qvideosurfaceformat.h>
#include <qcoreapplication.h>
#include <qthread.h>

QT_BEGIN_NAMESPACE

class QLibcameraCameraDataVideoOutput : public QLibcameraVideoOutput
                                    , public QLibcameraCameraSession::PreviewCallback
{
    Q_OBJECT
public:
    explicit QLibcameraCameraDataVideoOutput(QLibcameraCameraVideoRendererControl *control);
    ~QLibcameraCameraDataVideoOutput() override;

    LibcameraSurfaceHolder *surfaceHolder() override;

    bool isReady() override;

    void stop() override;

private Q_SLOTS:
    void onSurfaceCreated();
    void configureFormat();

private:
    void onFrameAvailable(const QVideoFrame &frame);
    void presentFrame();
    bool event(QEvent *);

    QLibcameraCameraVideoRendererControl *m_control;
    LibcameraSurfaceView *m_surfaceView;
    QMutex m_mutex;
    QVideoFrame::PixelFormat m_pixelFormat;
    QVideoFrame m_lastFrame;
};

QLibcameraCameraDataVideoOutput::QLibcameraCameraDataVideoOutput(QLibcameraCameraVideoRendererControl *control)
    : QLibcameraVideoOutput(control)
    , m_control(control)
    , m_pixelFormat(QVideoFrame::Format_Invalid)
{
    // The camera preview cannot be started unless we set a SurfaceTexture or a
    // SurfaceHolder. In this case we don't actually care about either of these, but since
    // we need to, we setup an offscreen dummy SurfaceView in order to be able to start
    // the camera preview. We'll then be able to use setPreviewCallbackWithBuffer() to
    // get the raw data.

    m_surfaceView = new LibcameraSurfaceView;

    connect(m_surfaceView, &LibcameraSurfaceView::surfaceCreated,
            this, &QLibcameraCameraDataVideoOutput::onSurfaceCreated);

    m_surfaceView->setGeometry(-1, -1, 1, 1);
    m_surfaceView->setVisible(true);

    connect(m_control->cameraSession(), &QLibcameraCameraSession::opened,
            this, &QLibcameraCameraDataVideoOutput::configureFormat);
    connect(m_control->surface(), &QAbstractVideoSurface::supportedFormatsChanged,
            this, &QLibcameraCameraDataVideoOutput::configureFormat);
    configureFormat();
}

QLibcameraCameraDataVideoOutput::~QLibcameraCameraDataVideoOutput()
{
    m_control->cameraSession()->setPreviewCallback(nullptr);
    delete m_surfaceView;
}

LibcameraSurfaceHolder *QLibcameraCameraDataVideoOutput::surfaceHolder()
{
    return m_surfaceView->holder();
}

bool QLibcameraCameraDataVideoOutput::isReady()
{
    return m_surfaceView->holder() && m_surfaceView->holder()->isSurfaceCreated();
}

void QLibcameraCameraDataVideoOutput::onSurfaceCreated()
{
    emit readyChanged(true);
}

void QLibcameraCameraDataVideoOutput::configureFormat()
{
    m_pixelFormat = QVideoFrame::Format_Invalid;

    if (!m_control->cameraSession()->camera())
        return;

    QList<QVideoFrame::PixelFormat> surfaceFormats = m_control->surface()->supportedPixelFormats();
    QList<LibcameraCamera::ImageFormat> previewFormats = m_control->cameraSession()->camera()->getSupportedPreviewFormats();
    for (int i = 0; i < surfaceFormats.size(); ++i) {
        QVideoFrame::PixelFormat pixFormat = surfaceFormats.at(i);
        LibcameraCamera::ImageFormat f = qt_libcameraImageFormatFromPixelFormat(pixFormat);
        if (previewFormats.contains(f)) {
            m_pixelFormat = pixFormat;
            break;
        }
    }

    if (m_pixelFormat == QVideoFrame::Format_Invalid) {
        m_control->cameraSession()->setPreviewCallback(nullptr);
        qWarning("The video surface is not compatible with any format supported by the camera");
    } else {
        m_control->cameraSession()->setPreviewCallback(this);

        if (m_control->cameraSession()->status() > QCamera::LoadedStatus)
            m_control->cameraSession()->camera()->stopPreview();

        m_control->cameraSession()->setPreviewFormat(qt_libcameraImageFormatFromPixelFormat(m_pixelFormat));

        if (m_control->cameraSession()->status() > QCamera::LoadedStatus)
            m_control->cameraSession()->camera()->startPreview();
    }
}

void QLibcameraCameraDataVideoOutput::stop()
{
    m_mutex.lock();
    m_lastFrame = QVideoFrame();
    m_mutex.unlock();

    if (m_control->surface() && m_control->surface()->isActive())
        m_control->surface()->stop();
}

void QLibcameraCameraDataVideoOutput::onFrameAvailable(const QVideoFrame &frame)
{
    m_mutex.lock();
    m_lastFrame = frame;
    m_mutex.unlock();

    if (thread() == QThread::currentThread())
        presentFrame();
    else
        QCoreApplication::postEvent(this, new QEvent(QEvent::User), Qt::HighEventPriority);
}

bool QLibcameraCameraDataVideoOutput::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        presentFrame();
        return true;
    }

    return QObject::event(e);
}

void QLibcameraCameraDataVideoOutput::presentFrame()
{
    Q_ASSERT(thread() == QThread::currentThread());

    QMutexLocker locker(&m_mutex);

    if (m_control->surface() && m_lastFrame.isValid() && m_lastFrame.pixelFormat() == m_pixelFormat) {

        if (m_control->surface()->isActive() && (m_control->surface()->surfaceFormat().pixelFormat() != m_lastFrame.pixelFormat()
                                                 || m_control->surface()->surfaceFormat().frameSize() != m_lastFrame.size())) {
            m_control->surface()->stop();
        }

        if (!m_control->surface()->isActive()) {
            QVideoSurfaceFormat format(m_lastFrame.size(), m_lastFrame.pixelFormat(), m_lastFrame.handleType());
            // Front camera frames are automatically mirrored when using SurfaceTexture or SurfaceView,
            // but the buffers we get from the data callback are not. Tell the QAbstractVideoSurface
            // that it needs to mirror the frames.
            if (m_control->cameraSession()->camera()->getFacing() == LibcameraCamera::CameraFacingFront)
                format.setProperty("mirrored", true);

            m_control->surface()->start(format);
        }

        if (m_control->surface()->isActive())
            m_control->surface()->present(m_lastFrame);
    }

    m_lastFrame = QVideoFrame();
}


QLibcameraCameraVideoRendererControl::QLibcameraCameraVideoRendererControl(QLibcameraCameraSession *session, QObject *parent)
    : QVideoRendererControl(parent)
    , m_cameraSession(session)
    , m_surface(0)
    , m_textureOutput(0)
    , m_dataOutput(0)
{
}

QLibcameraCameraVideoRendererControl::~QLibcameraCameraVideoRendererControl()
{
    m_cameraSession->setVideoOutput(0);
}

QAbstractVideoSurface *QLibcameraCameraVideoRendererControl::surface() const
{
    return m_surface;
}

void QLibcameraCameraVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface == surface)
        return;

    m_surface = surface;
    QLibcameraVideoOutput *oldOutput = m_textureOutput ? static_cast<QLibcameraVideoOutput*>(m_textureOutput)
                                                     : static_cast<QLibcameraVideoOutput*>(m_dataOutput);
    QLibcameraVideoOutput *newOutput = 0;

    if (m_surface) {
        if (!m_surface->supportedPixelFormats(QAbstractVideoBuffer::GLTextureHandle).isEmpty()) {
            if (!m_textureOutput) {
                m_dataOutput = 0;
                newOutput = m_textureOutput = new QLibcameraTextureVideoOutput(this);
            }
        } else if (!m_dataOutput) {
            m_textureOutput = 0;
            newOutput = m_dataOutput = new QLibcameraCameraDataVideoOutput(this);
        }

        if (m_textureOutput)
            m_textureOutput->setSurface(m_surface);
    }

    if (newOutput != oldOutput) {
        m_cameraSession->setVideoOutput(newOutput);
        delete oldOutput;
    }
}

QT_END_NAMESPACE

#include "qlibcameracameravideorenderercontrol.moc"

