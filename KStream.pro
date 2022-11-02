QT += quick gui-private

SOURCES += \
        gstqmlrenderer.cpp \
        main.cpp

resources.files = main.qml Content.qml
resources.prefix = /$${TARGET}
RESOURCES += resources

TRANSLATIONS += \
    KStream_fr_FR.ts
CONFIG += lrelease
CONFIG += embed_translations
CONFIG += link_pkgconfig

DEFINES += GST_USE_UNSTABLE_API

PKGCONFIG += gstreamer-1.0 gstreamer-app-1.0 gstreamer-gl-1.0 egl gstreamer-gl-egl-1.0 wayland-client

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    gstqmlrenderer.h

DISTFILES += \
    Content.qml
