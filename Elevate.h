#pragma once
#include <QString>

// Hands a non-elevated OpenRGB off to an elevated copy (via a scheduled task) so admin-only sensors work.
namespace Elevate
{
    enum Result { Continue, ContinueHidden, Quit };
    Result Run(const QString& cfgDir);
    void   MarkHealthy(const QString& cfgDir);  // elevated copy calls this once it has clearly survived startup
}
