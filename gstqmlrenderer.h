#ifndef GSTQMLRENDERER_H
#define GSTQMLRENDERER_H

#include <QObject>
#include <QUrl>
#include <gst/gst.h>
class QSurfaceFormat;
class QOpenGLContext;
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstGLContext GstGLContext;
class QQmlEngine;
class QQuickRenderControl;
class QQuickWindow;
class QQmlComponent;
class QQuickItem;
class QOffscreenSurface;
class QOpenGLFramebufferObject;
class QTimer;
typedef struct _GstVideoInfo GstVideoInfo;

class GstQmlRenderer : public QObject
{
    friend gboolean sync_bus_call (GstBus *, GstMessage *, GstQmlRenderer *);
    Q_OBJECT
public:
    explicit GstQmlRenderer(QObject *parent = nullptr);
    void start();

private:
    QUrl _qmlUrl = QUrl("qrc:/KStream/Content.qml");
    int _width = 640;
    int _height = 480;
    int _framerate = 60;
    QSurfaceFormat * _surfaceFormat;
    QOpenGLContext * _qcontext;
    GstGLDisplay * _display;
    GstGLContext * _gstQtContext;
    GstGLContext * _gstGstContext;
    uint _gstFbo;
    QQmlEngine * _qmlEngine;
    QQuickRenderControl * _renderControl;
    QQuickWindow * _quickWindow;
    QQmlComponent * _qmlComponent;
    QQuickItem * _rootItem;
    QOffscreenSurface * _offscreenSurface;
    QOpenGLFramebufferObject * _fbo;
    GstElement * _pipeline;
    GstElement * _appSource;
    GstElement * _gldownload;
    GstElement * _queue;
    GstElement * _vaapisink;
    QTimer * _renderTimer;
    GstAllocator * _gstMemoryAllocator;
    GstVideoInfo * _gstVideoInfo;
    qint64 _startTime;

    void createGlContexts();
    void createWindow();
    void createFramebuffer();
    void setupRendering();
    void createPipeline();
    void startPipeline();
    void startRendering();
    void render();

signals:

};

#endif // GSTQMLRENDERER_H
