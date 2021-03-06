/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandcompositor.h"

#include "qwaylandinput.h"

#include "wayland_wrapper/qwlcompositor_p.h"
#include "wayland_wrapper/qwlsurface_p.h"
#include "wayland_wrapper/qwlinputdevice_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

#include <QtGui/QDesktopServices>

#include <QDebug>

#ifdef QT_COMPOSITOR_QUICK
#include "qwaylandsurfaceitem.h"
#endif

QT_BEGIN_NAMESPACE

QWaylandCompositor::QWaylandCompositor(QWindow *window, const char *socketName)
    : m_compositor(0)
    , m_toplevel_window(window)
    , m_socket_name(socketName)
{
    QStringList arguments = QCoreApplication::instance()->arguments();

    int socketArg = arguments.indexOf(QLatin1String("--wayland-socket-name"));
    if (socketArg != -1 && socketArg + 1 < arguments.size())
        m_socket_name = arguments.at(socketArg + 1).toLocal8Bit();

    m_compositor = new QtWayland::Compositor(this);
#ifdef QT_COMPOSITOR_QUICK
    qmlRegisterType<QWaylandSurfaceItem>("WaylandCompositor", 1, 0, "WaylandSurfaceItem");
    qmlRegisterType<QWaylandSurface>("WaylandCompositor", 1, 0, "WaylandSurface");
#else
    qRegisterMetaType<QWaylandSurface*>("WaylandSurface*");
#endif
    m_compositor->initializeHardwareIntegration();
    m_compositor->initializeWindowManagerProtocol();
    m_compositor->initializeDefaultInputDevice();
}

QWaylandCompositor::~QWaylandCompositor()
{
    delete m_compositor;
}

struct wl_display *QWaylandCompositor::waylandDisplay() const
{
    return m_compositor->wl_display();
}
void QWaylandCompositor::frameFinished(QWaylandSurface *surface)
{
    QtWayland::Surface *surfaceImpl = surface? surface->handle():0;
    m_compositor->frameFinished(surfaceImpl);
}

void QWaylandCompositor::destroyClientForSurface(QWaylandSurface *surface)
{
    destroyClient(surface->client());
}

void QWaylandCompositor::destroyClient(WaylandClient *client)
{
    m_compositor->destroyClient(client);
}

QList<QWaylandSurface *> QWaylandCompositor::surfacesForClient(WaylandClient* c) const
{
    wl_client *client = static_cast<wl_client *>(c);

    QList<QtWayland::Surface *> surfaces = m_compositor->surfaces();

    QList<QWaylandSurface *> result;

    for (int i = 0; i < surfaces.count(); ++i) {
        if (surfaces.at(i)->resource()->client() == client) {
            result.append(surfaces.at(i)->waylandSurface());
        }
    }

    return result;
}

bool QWaylandCompositor::setDirectRenderSurface(QWaylandSurface *surface, QOpenGLContext *context)
{
    return m_compositor->setDirectRenderSurface(surface ? surface->handle() : 0, context);
}

QWaylandSurface *QWaylandCompositor::directRenderSurface() const
{
    QtWayland::Surface *surf = m_compositor->directRenderSurface();
    return surf ? surf->waylandSurface() : 0;
}

QWindow * QWaylandCompositor::window() const
{
    return m_toplevel_window;
}

void QWaylandCompositor::cleanupGraphicsResources()
{
    m_compositor->cleanupGraphicsResources();
}

void QWaylandCompositor::surfaceAboutToBeDestroyed(QWaylandSurface *surface)
{
    Q_UNUSED(surface);
}

/*!
    Override this to handle QDesktopServices::openUrl() requests from the clients.

    The default implementation simply forwards the request to QDesktopServices::openUrl().
*/
void QWaylandCompositor::openUrl(WaylandClient *client, const QUrl &url)
{
    Q_UNUSED(client);
    QDesktopServices::openUrl(url);
}

QtWayland::Compositor * QWaylandCompositor::handle() const
{
    return m_compositor;
}

void QWaylandCompositor::setRetainedSelectionEnabled(bool enable)
{
    if (enable)
        m_compositor->setRetainedSelectionWatcher(retainedSelectionChanged, this);
    else
        m_compositor->setRetainedSelectionWatcher(0, 0);
}

void QWaylandCompositor::retainedSelectionChanged(QMimeData *mimeData, void *param)
{
    QWaylandCompositor *self = static_cast<QWaylandCompositor *>(param);
    self->retainedSelectionReceived(mimeData);
}

void QWaylandCompositor::retainedSelectionReceived(QMimeData *)
{
}

void QWaylandCompositor::overrideSelection(QMimeData *data)
{
    m_compositor->overrideSelection(data);
}

void QWaylandCompositor::setClientFullScreenHint(bool value)
{
    m_compositor->setClientFullScreenHint(value);
}

const char *QWaylandCompositor::socketName() const
{
    if (m_socket_name.isEmpty())
        return 0;
    return m_socket_name.constData();
}

/*!
  Set the screen orientation based on accelerometer data or similar.
*/
void QWaylandCompositor::setScreenOrientation(Qt::ScreenOrientation orientation)
{
    m_compositor->setScreenOrientation(orientation);
}

void QWaylandCompositor::setOutputGeometry(const QRect &geometry)
{
    m_compositor->setOutputGeometry(geometry);
}

QRect QWaylandCompositor::outputGeometry() const
{
    return m_compositor->outputGeometry();
}

void QWaylandCompositor::setOutputRefreshRate(int rate)
{
    m_compositor->setOutputRefreshRate(rate);
}

int QWaylandCompositor::outputRefreshRate() const
{
    return m_compositor->outputRefreshRate();
}

QWaylandInputDevice *QWaylandCompositor::defaultInputDevice() const
{
    return m_compositor->defaultInputDevice()->handle();
}

bool QWaylandCompositor::isDragging() const
{
    return m_compositor->isDragging();
}

void QWaylandCompositor::sendDragMoveEvent(const QPoint &global, const QPoint &local,
                                          QWaylandSurface *surface)
{
    m_compositor->sendDragMoveEvent(global, local, surface ? surface->handle() : 0);
}

void QWaylandCompositor::sendDragEndEvent()
{
    m_compositor->sendDragEndEvent();
}

void QWaylandCompositor::setCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY)
{
    Q_UNUSED(surface);
    Q_UNUSED(hotspotX);
    Q_UNUSED(hotspotY);
}

void QWaylandCompositor::enableSubSurfaceExtension()
{
    m_compositor->enableSubSurfaceExtension();
}

void QWaylandCompositor::enableTouchExtension()
{
    // nothing to do here
}

void QWaylandCompositor::configureTouchExtension(TouchExtensionFlags flags)
{
    m_compositor->configureTouchExtension(flags);
}

QT_END_NAMESPACE
