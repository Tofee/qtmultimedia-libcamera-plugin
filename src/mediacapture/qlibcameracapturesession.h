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

#ifndef QLIBCAMERACAPTURESESSION_H
#define QLIBCAMERACAPTURESESSION_H

#include <qobject.h>
#include <qmediarecorder.h>
#include <qurl.h>
#include <qelapsedtimer.h>
#include <qtimer.h>
#include <private/qmediastoragelocation_p.h>

QT_BEGIN_NAMESPACE

class QLibcameraCameraSession;

class QLibcameraCaptureSession : public QObject
{
    Q_OBJECT
public:
    explicit QLibcameraCaptureSession(QLibcameraCameraSession *cameraSession = 0);
    ~QLibcameraCaptureSession();

    QList<QSize> supportedResolutions() const { return m_supportedResolutions; }
    QList<qreal> supportedFrameRates() const { return m_supportedFramerates; }

    QString audioInput() const { return m_audioInput; }
    void setAudioInput(const QString &input);

    QUrl outputLocation() const;
    bool setOutputLocation(const QUrl &location);

    QMediaRecorder::State state() const;
    void setState(QMediaRecorder::State state);

    QMediaRecorder::Status status() const;

    qint64 duration() const;

    QString containerFormat() const { return m_containerFormat; }
    void setContainerFormat(const QString &format);

    QAudioEncoderSettings audioSettings() const { return m_audioSettings; }
    void setAudioSettings(const QAudioEncoderSettings &settings);

    QVideoEncoderSettings videoSettings() const { return m_videoSettings; }
    void setVideoSettings(const QVideoEncoderSettings &settings);

    void applySettings();

Q_SIGNALS:
    void audioInputChanged(const QString& name);
    void stateChanged(QMediaRecorder::State state);
    void statusChanged(QMediaRecorder::Status status);
    void durationChanged(qint64 position);
    void actualLocationChanged(const QUrl &location);
    void error(int error, const QString &errorString);

private Q_SLOTS:
    void updateDuration();
    void onCameraOpened();

    void onError(int what, int extra);
    void onInfo(int what, int extra);

private:
    struct CaptureProfile {
        LibcameraMediaRecorder::OutputFormat outputFormat;
        QString outputFileExtension;

        LibcameraMediaRecorder::AudioEncoder audioEncoder;
        int audioBitRate;
        int audioChannels;
        int audioSampleRate;

        LibcameraMediaRecorder::VideoEncoder videoEncoder;
        int videoBitRate;
        int videoFrameRate;
        QSize videoResolution;

        bool isNull;

        CaptureProfile()
            : outputFormat(LibcameraMediaRecorder::MPEG_4)
            , outputFileExtension(QLatin1String("mp4"))
            , audioEncoder(LibcameraMediaRecorder::DefaultAudioEncoder)
            , audioBitRate(128000)
            , audioChannels(2)
            , audioSampleRate(44100)
            , videoEncoder(LibcameraMediaRecorder::DefaultVideoEncoder)
            , videoBitRate(1)
            , videoFrameRate(-1)
            , videoResolution(320, 240)
            , isNull(true)
        { }
    };

    CaptureProfile getProfile(int id);

    void start();
    void stop(bool error = false);

    void setStatus(QMediaRecorder::Status status);

    void updateViewfinder();
    void restartViewfinder();

    LibcameraMediaRecorder *m_mediaRecorder;
    QLibcameraCameraSession *m_cameraSession;

    QString m_audioInput;
    LibcameraMediaRecorder::AudioSource m_audioSource;

    QMediaStorageLocation m_mediaStorageLocation;

    QElapsedTimer m_elapsedTime;
    QTimer m_notifyTimer;
    qint64 m_duration;

    QMediaRecorder::State m_state;
    QMediaRecorder::Status m_status;
    QUrl m_requestedOutputLocation;
    QUrl m_usedOutputLocation;
    QUrl m_actualOutputLocation;

    CaptureProfile m_defaultSettings;

    QString m_containerFormat;
    QAudioEncoderSettings m_audioSettings;
    QVideoEncoderSettings m_videoSettings;
    bool m_containerFormatDirty;
    bool m_videoSettingsDirty;
    bool m_audioSettingsDirty;
    LibcameraMediaRecorder::OutputFormat m_outputFormat;
    LibcameraMediaRecorder::AudioEncoder m_audioEncoder;
    LibcameraMediaRecorder::VideoEncoder m_videoEncoder;

    QList<QSize> m_supportedResolutions;
    QList<qreal> m_supportedFramerates;
};

QT_END_NAMESPACE

#endif // QLIBCAMERACAPTURESESSION_H
