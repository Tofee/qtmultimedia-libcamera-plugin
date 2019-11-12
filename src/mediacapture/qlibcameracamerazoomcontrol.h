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

#ifndef QLIBCAMERACAMERAZOOMCONTROL_H
#define QLIBCAMERACAMERAZOOMCONTROL_H

#include <qcamerazoomcontrol.h>
#include <qcamera.h>

QT_BEGIN_NAMESPACE

class QLibcameraCameraSession;

class QLibcameraCameraZoomControl : public QCameraZoomControl
{
    Q_OBJECT
public:
    explicit QLibcameraCameraZoomControl(QLibcameraCameraSession *session);

    qreal maximumOpticalZoom() const override;
    qreal maximumDigitalZoom() const override;
    qreal requestedOpticalZoom() const override;
    qreal requestedDigitalZoom() const override;
    qreal currentOpticalZoom() const override;
    qreal currentDigitalZoom() const override;
    void zoomTo(qreal optical, qreal digital) override;

private Q_SLOTS:
    void onCameraOpened();

private:
    QLibcameraCameraSession *m_cameraSession;

    qreal m_maximumZoom;
    QList<int> m_zoomRatios;
    qreal m_requestedZoom;
    qreal m_currentZoom;
};

QT_END_NAMESPACE

#endif // QLIBCAMERACAMERAZOOMCONTROL_H
