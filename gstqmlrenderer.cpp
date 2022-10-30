#include "gstqmlrenderer.h"
#include <QQmlEngine>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QUrl>
#include <QQuickItem>
#include <QOpenGLFramebufferObject>
#include <QQuickRenderTarget>
#include "gst/gst.h"
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/gl/egl/egl.h>
#include <EGL/egl.h>
#include <QTimer>
#include <gst/app/app.h>
#include <QOpenGLFunctions>
#include <QDateTime>

GstQmlRenderer::GstQmlRenderer(QObject *parent)
    : QObject{parent}
{

}

void GstQmlRenderer::createGlContexts() {
    _surfaceFormat = new QSurfaceFormat;
    _surfaceFormat->setDepthBufferSize(16);
    _surfaceFormat->setStencilBufferSize(8);
    _qcontext = new QOpenGLContext();
    _qcontext->setFormat(*_surfaceFormat);
    _qcontext->create();
    _offscreenSurface = new QOffscreenSurface();
    _offscreenSurface->setFormat(*_surfaceFormat);
    _offscreenSurface->create();
    QNativeInterface::QEGLContext * eglContext = _qcontext->nativeInterface<QNativeInterface::QEGLContext>();
    g_setenv("GST_GL_PLATFORM", "egl", false);
    _display = (GstGLDisplay*)gst_gl_display_egl_new_with_egl_display(eglContext->display());
    gst_gl_display_filter_gl_api(_display, GST_GL_API_GLES2);
    _gstQtContext = gst_gl_context_new_wrapped(_display, (guintptr) eglContext->nativeContext(), GST_GL_PLATFORM_EGL, GST_GL_API_GLES2);
    GError * error = NULL;
    gst_gl_context_activate(_gstQtContext, true);
    _qcontext->makeCurrent(_offscreenSurface);
    if(!gst_gl_context_fill_info(_gstQtContext, &error)) {
        qDebug() << error->message;
    }
    _qcontext->doneCurrent();
    gst_gl_context_activate(_gstQtContext, false);
    _gstGstContext = gst_gl_context_new(_display);
    if(!gst_gl_context_create(_gstGstContext, _gstQtContext, &error)) {
        qDebug() << error->message;
    }
}

void GstQmlRenderer::createWindow() {
    _qmlEngine = new QQmlEngine();
    _renderControl = new QQuickRenderControl();
    _quickWindow = new QQuickWindow(_renderControl);
    if (!_qmlEngine->incubationController()) {
        _qmlEngine->setIncubationController(_quickWindow->incubationController());
    }
    _renderControl->initialize();
    _qmlComponent = new QQmlComponent(_qmlEngine, _qmlUrl, QQmlComponent::PreferSynchronous);
    if( _qmlComponent->isError() ) {
        qDebug() << _qmlComponent->errorString();
    }
    QObject * rootObject = _qmlComponent->create();
    _rootItem = qobject_cast<QQuickItem *>(rootObject);
    _rootItem->setParentItem(_quickWindow->contentItem());
    _rootItem->setWidth(_width);
    _rootItem->setHeight(_height);
    _quickWindow->setGeometry(0, 0, _width, _height);
}

void GstQmlRenderer::createFramebuffer() {
    _fbo = new QOpenGLFramebufferObject(_width,_height, QOpenGLFramebufferObject::CombinedDepthStencil, GL_TEXTURE_2D, GL_RGBA8);
    _fbo->takeTexture();
}

void GstQmlRenderer::setupRendering() {
    gst_gl_memory_init_once();
    _gstMemoryAllocator = (GstAllocator *)gst_gl_memory_allocator_get_default (_gstGstContext);
    //_gstMemoryAllocator = gst_allocator_find(GST_GL_MEMORY_ALLOCATOR_NAME);
}

gboolean sync_bus_call (GstBus * bus, GstMessage * msg, GstQmlRenderer * r)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:
    {
      const gchar *
          context_type;

      gst_message_parse_context_type (msg, &context_type);
      g_print ("got need context %s\n", context_type);

      if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext *display_context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display (display_context, r->_display);
        gst_element_set_context (GST_ELEMENT (msg->src), display_context);
      } else if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
        GstContext *app_context = gst_context_new ("gst.gl.app_context", TRUE);
        GstStructure *s = gst_context_writable_structure (app_context);
        gst_structure_set (s, "context", GST_TYPE_GL_CONTEXT, r->_gstGstContext, NULL);
        gst_element_set_context (GST_ELEMENT (msg->src), app_context);
      }
      break;
    }
    default:
      break;
  }
  return FALSE;
}

void GstQmlRenderer::createPipeline() {
    _pipeline = gst_pipeline_new("renderer");
    _appSource = gst_element_factory_make("appsrc", "renderer-appsrc");
    g_object_set(_appSource, "is-live", true, NULL);
    g_object_set(_appSource, "format", GST_FORMAT_TIME, NULL);
    g_object_set(_appSource, "do-timestamp", true, NULL);
    GstCaps * appsourceCaps = gst_caps_new_simple ("video/x-raw", \
                                                   "format", G_TYPE_STRING, "RGBA", \
                                                   "width", G_TYPE_INT, _width, \
                                                   "height", G_TYPE_INT, _height, \
                                                   "framerate", GST_TYPE_FRACTION, _framerate, 1, \
                                                   "texture-target", G_TYPE_STRING, "2D", \
                                                   NULL
                                                   );
    GstCapsFeatures * features = gst_caps_features_new_single(GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    gst_caps_set_features_simple (appsourceCaps, features);
    _gstVideoInfo = gst_video_info_new_from_caps(appsourceCaps);
    g_object_set(_appSource, "caps", appsourceCaps, NULL);
    gst_caps_unref(appsourceCaps);
    _gldownload = gst_element_factory_make("gldownload", "renderer-download");
    _queue = gst_element_factory_make("queue", "renderer-queue");
    _vaapisink = gst_element_factory_make("vaapisink", "renderer-sink");
    gst_bin_add_many(GST_BIN (_pipeline), _appSource, _gldownload, _queue, _vaapisink, NULL);
    gst_element_link_many(_appSource, _gldownload, _queue, _vaapisink, NULL);
    GstBus * bus = gst_pipeline_get_bus (GST_PIPELINE (_pipeline));
    gst_bus_enable_sync_message_emission (bus);
    g_signal_connect (bus, "sync-message", G_CALLBACK (sync_bus_call), this);
    gst_object_unref (bus);
}

void GstQmlRenderer::startPipeline() {
    gst_element_set_state(_pipeline, GstState::GST_STATE_PLAYING);
}

void GstQmlRenderer::startRendering() {
    _startTime = QDateTime::currentMSecsSinceEpoch();
    _renderTimer = new QTimer(this);
    connect(_renderTimer, &QTimer::timeout, this, &GstQmlRenderer::render);
    _renderTimer->setTimerType(Qt::TimerType::PreciseTimer);
    _renderTimer->setSingleShot(false);
    _renderTimer->start(1000/_framerate);
}

typedef struct _TextureContext {
    GLuint textureId;
    GstVideoInfo * gstVideoInfo;
    GstGLBaseMemoryAllocator * allocator;
    GstElement * appSource;
    GstGLContext * ctx;
    guint64 pts;
} TextureContext;

void deleteTexture(GstGLContext * ctx, gpointer data) {
    TextureContext * deleteCtx = (TextureContext *)data;
    ctx->gl_vtable->DeleteTextures(1, &deleteCtx->textureId);
    delete deleteCtx;
}

void notifyTextureDestruction(gpointer data) {
    TextureContext * ctx = (TextureContext*)data;
    gst_gl_context_thread_add(ctx->ctx, deleteTexture, ctx);
}

void pushTexture(GstGLContext * ctx, gpointer data) {
    TextureContext * pushCtx = (TextureContext *)data;
    glBindTexture(GL_TEXTURE_2D, pushCtx->textureId);
    GstBuffer *buffer = gst_buffer_new();
    GstGLVideoAllocationParams * params = gst_gl_video_allocation_params_new_wrapped_texture(
                ctx,
                NULL,
                pushCtx->gstVideoInfo,
                0,
                NULL,
                GST_GL_TEXTURE_TARGET_2D,
                GST_GL_RGBA8,
                pushCtx->textureId,
                data,
                notifyTextureDestruction);
    GstGLFormat formats[1];
    gpointer textures[1];
    formats[0] = GST_GL_RGBA8;
    textures[0] = (gpointer)pushCtx->textureId;
    if(!gst_gl_memory_setup_buffer((GstGLMemoryAllocator *)pushCtx->allocator, buffer, params, formats, textures, 1) ) {
        qDebug() << "can't setup buffer";
    }

    GstGLSyncMeta *sync_meta = gst_buffer_get_gl_sync_meta (buffer);
    if (sync_meta) {
      gst_gl_sync_meta_set_sync_point (sync_meta, ctx);
      gst_gl_sync_meta_wait (sync_meta, ctx);
    }


    gst_app_src_push_buffer(GST_APP_SRC(pushCtx->appSource), buffer);
}

void GstQmlRenderer::render() {
    _qcontext->makeCurrent(_offscreenSurface);
    _fbo->bind();
    _quickWindow->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(_fbo->texture(), QSize(_width, _height)));
    _renderControl->beginFrame();
    _renderControl->polishItems();
    _renderControl->sync();
    _renderControl->render();
    _renderControl->endFrame();
    TextureContext * pushContext = new TextureContext;
    pushContext->textureId = _fbo->takeTexture();
    _qcontext->functions()->glBindTexture(GST_GL_TEXTURE_TARGET_2D, 0);
    _qcontext->functions()->glFinish();
    pushContext->gstVideoInfo = _gstVideoInfo;
    pushContext->allocator = (GstGLBaseMemoryAllocator*)_gstMemoryAllocator;
    pushContext->appSource = _appSource;
    pushContext->ctx = _gstGstContext;
    pushContext->pts = QDateTime::currentMSecsSinceEpoch() - _startTime;
    _qcontext->doneCurrent();
    gst_gl_context_thread_add(_gstGstContext, pushTexture, pushContext);
}

void GstQmlRenderer::start() {
    createGlContexts();
    createWindow();
    createFramebuffer();
    setupRendering();
    createPipeline();
    startPipeline();
    startRendering();
}
