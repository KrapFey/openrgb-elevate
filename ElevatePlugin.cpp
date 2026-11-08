#include "ElevatePlugin.h"
#include <QImage>
#include "Elevate.h"
#include <QApplication>
#include <QEvent>
#include <QTimer>
#include <QWidget>

OpenRGBPluginInfo ElevatePlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name            = "Run as Administrator";
    info.Description     = "Starts OpenRGB elevated (no UAC prompt) so admin-only sensors work, e.g. CPU temperatures in Hardware Sync";
    info.Version         = PLUGIN_VERSION;
    info.Commit          = PLUGIN_COMMIT;
    info.URL             = PLUGIN_URL;
    info.Location        = 0xFFFFFFFF;  // invalid on purpose: no tab, this plugin has no UI
    info.Label           = "Elevate";
    info.ProtocolVersion = 0;
    info.Icon            = QImage(":/openrgb-elevate/icon.png");
    return info;
}

void ElevatePlugin::Load(OpenRGBPluginAPIInterface* api)
{
    QString cfgDir = QString::fromStdWString(api->GetConfigurationDirectory().wstring());
    switch(Elevate::Run(cfgDir))
    {
        case Elevate::Quit:           QTimer::singleShot(0, qApp, &QCoreApplication::quit); return;  // elevated copy takes over
        case Elevate::ContinueHidden: hideMainOnShow = true; qApp->installEventFilter(this); break;  // keep --startminimized
        case Elevate::Continue:       break;
    }
    QTimer::singleShot(15000, qApp, [cfgDir] { Elevate::MarkHealthy(cfgDir); });  // no-op unless elevated
}

bool ElevatePlugin::eventFilter(QObject* obj, QEvent* event)
{
    if(hideMainOnShow && event->type() == QEvent::Show && obj->isWidgetType() && obj->inherits("QMainWindow"))
    {
        hideMainOnShow = false;
        qApp->removeEventFilter(this);
        QTimer::singleShot(0, static_cast<QWidget*>(obj), &QWidget::hide);  // back to tray, like --startminimized
    }
    return false;
}

QWidget* ElevatePlugin::GetWidget()
{
    if(!widget) widget = new QWidget();  // never shown (no tab), returned only so the host never gets null
    return widget;
}

void ElevatePlugin::Unload()
{
    qApp->removeEventFilter(this);
}
