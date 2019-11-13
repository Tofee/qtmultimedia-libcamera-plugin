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

#include "qlibcameramultimediautils.h"
#include "qlibcameraglobal.h"

#include "libcamera/libcamera.h"

#include "libdrm/drm_fourcc.h"

#include <QVideoFrame>
#include <qlist.h>

QT_BEGIN_NAMESPACE

int qt_findClosestValue(const QList<int> &list, int value)
{
    if (list.size() < 2)
        return 0;

    int begin = 0;
    int end = list.size() - 1;
    int pivot = begin + (end - begin) / 2;
    int v = list.at(pivot);

    while (end - begin > 1) {
        if (value == v)
            return pivot;

        if (value > v)
            begin = pivot;
        else
            end = pivot;

        pivot = begin + (end - begin) / 2;
        v = list.at(pivot);
    }

    return value - v >= list.at(pivot + 1) - value ? pivot + 1 : pivot;
}

bool qt_sizeLessThan(const QSize &s1, const QSize &s2)
{
    return s1.width() * s1.height() < s2.width() * s2.height();
}

QVideoFrame::PixelFormat QtPixelFormatFromLibcameraPixelFormat(libcamera::PixelFormat format)
{
    switch (format) {
    case DRM_FORMAT_RGB565:
        return QVideoFrame::Format_RGB565;
    case DRM_FORMAT_NV21:
        return QVideoFrame::Format_NV21;
        /*
    case DRM_FORMAT_YUY2:
        return QVideoFrame::Format_YUYV;
    case DRM_FORMAT_JPEG:
        return QVideoFrame::Format_Jpeg;
    case DRM_FORMAT_YV12:
        return QVideoFrame::Format_YV12;
        */
    default:
        return QVideoFrame::Format_Invalid;
    }
}

libcamera::PixelFormat LibcameraPixelFormatFromQtPixelFormat(QVideoFrame::PixelFormat format)
{
    switch (format) {
    case QVideoFrame::Format_RGB565:
        return DRM_FORMAT_RGB565;
    case QVideoFrame::Format_NV21:
        return DRM_FORMAT_NV21;
        /*
    case QVideoFrame::Format_YUYV:
        return DRM_FORMAT_YUY2;
    case QVideoFrame::Format_Jpeg:
        return DRM_FORMAT_JPEG;
    case QVideoFrame::Format_YV12:
        return DRM_FORMAT_YV12;
        */
    default:
        return DRM_FORMAT_INVALID;
    }
}

QT_END_NAMESPACE
