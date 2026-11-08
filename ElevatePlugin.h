#pragma once
#include <QObject>
#include "OpenRGBPluginInterface.h"

// Makes OpenRGB run elevated so admin-only sensors (CPU temperatures for Hardware Sync, via PawnIO) work.
// See Elevate.cpp for how the handoff works.
class ElevatePlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "ElevatePlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override { return OPENRGB_PLUGIN_API_VERSION; }
    void                Load(OpenRGBPluginAPIInterface* api) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override { return nullptr; }
    void                Unload() override;

    void                OnProfileAboutToLoad() override {}
    void                OnProfileLoad(nlohmann::json) override {}
    nlohmann::json      OnProfileSave() override { return nlohmann::json(); }
    unsigned char*      OnSDKCommand(unsigned int, unsigned char*, unsigned int*) override { return nullptr; }
    void                ProfileManagerUpdated(unsigned int) override {}
    void                ResourceManagerUpdated(unsigned int) override {}
    void                SettingsManagerUpdated(unsigned int) override {}

protected:
    bool                eventFilter(QObject* obj, QEvent* event) override;

private:
    QWidget*            widget         = nullptr;
    bool                hideMainOnShow = false;
};
