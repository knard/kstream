#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QLocale>
#include <QTranslator>
#include <gst/gst.h>
#include "gstqmlrenderer.h"

int main(int argc, char *argv[])
{
    gst_init (&argc, &argv);
    QGuiApplication app(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "KStream_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    GstQmlRenderer renderer;
    QQmlApplicationEngine engine;
    const QUrl url(u"qrc:/KStream/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [&renderer, url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
        renderer.start();
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
