#include "Elevate.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <windows.h>
#include <shellapi.h>

// Why: plugins run inside the OpenRGB window process. Sensors that go through PawnIO (CPU temps for the
// Hardware Sync plugin) only work when that process is elevated, and OpenRGB starts non-elevated from
// the Startup folder. A scheduled task with "highest privileges" can start it elevated without a UAC
// prompt, so a non-elevated instance hands off to the task and quits.
//
// ponytail: only --startminimized is carried over to the elevated instance; other CLI args are dropped.

static const wchar_t* TASK_NAME = L"OpenRGB - elevated";

static bool IsElevated()
{
    HANDLE          token = nullptr;
    TOKEN_ELEVATION elev  = {};
    DWORD           size  = 0;
    bool            ok    = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)
                         && GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size);
    if(token) CloseHandle(token);
    return ok && elev.TokenIsElevated;
}

// Runs a console tool without flashing a console window; returns its exit code (-1 if it failed to start).
static int RunHidden(const QString& cmdline)
{
    std::wstring        cmd = cmdline.toStdWString();
    STARTUPINFOW        si  = { sizeof(si) };
    PROCESS_INFORMATION pi  = {};
    if(!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

// Creates the task through one UAC prompt (schtasks needs admin to register a highest-privilege task).
static bool CreateTask(const QString& cfgDir)
{
    QString user = qEnvironmentVariable("USERDOMAIN") + "\\" + qEnvironmentVariable("USERNAME");
    QString exe  = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QString xml  = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <Principals><Principal id=\"Author\"><UserId>%1</UserId><LogonType>InteractiveToken</LogonType>"
        "<RunLevel>HighestAvailable</RunLevel></Principal></Principals>\n"
        "  <Settings><MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>"
        "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>5</Priority></Settings>\n"
        // WorkingDirectory matters: tasks default to System32, and Hardware Sync's sensor library crashes
        // when started elevated from there.
        "  <Actions Context=\"Author\"><Exec><Command>%2</Command><WorkingDirectory>%3</WorkingDirectory></Exec></Actions>\n"
        "</Task>\n").arg(user.toHtmlEscaped(), exe.toHtmlEscaped(),
                         QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toHtmlEscaped());

    QString xmlPath = cfgDir + "/elevate-task.xml";
    QFile f(xmlPath);
    if(!f.open(QIODevice::WriteOnly)) return false;
    f.write("\xFF\xFE");  // UTF-16 LE BOM: schtasks expects UTF-16 XML
    f.write(reinterpret_cast<const char*>(xml.utf16()), xml.size() * 2);
    f.close();

    std::wstring params = QString("/create /f /tn \"%1\" /xml \"%2\"")
                              .arg(QString::fromWCharArray(TASK_NAME), QDir::toNativeSeparators(xmlPath)).toStdWString();
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = L"schtasks.exe";
    sei.lpParameters = params.c_str();
    sei.nShow        = SW_HIDE;
    bool ok = ShellExecuteExW(&sei) && sei.hProcess;  // false if the user cancels UAC
    DWORD code = 1;
    if(ok)
    {
        WaitForSingleObject(sei.hProcess, 30000);
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
    }
    QFile::remove(xmlPath);
    return ok && code == 0;
}

void Elevate::MarkHealthy(const QString& cfgDir)
{
    if(!IsElevated()) return;
    QFile f(cfgDir + "/elevate-ok");
    if(f.open(QIODevice::WriteOnly)) f.write("ok");  // write, so the timestamp moves even if it already exists
}

Elevate::Result Elevate::Run(const QString& cfgDir)
{
    QString hop      = cfgDir + "/elevate-hop";      // written just before handing off: "min" or ""
    QString declined = cfgDir + "/elevate-declined"; // user said no: never ask again (delete file to re-ask)

    if(IsElevated())
    {
        QFile f(hop);
        if(!f.open(QIODevice::ReadOnly)) return Continue;
        QStringList parts = QString::fromUtf8(f.readAll()).split(' ', Qt::SkipEmptyParts);  // "<pid> [min]"
        f.close();
        QFile::remove(hop);

        // Wait for the non-elevated copy to exit before other plugins load: two Hardware Sync instances
        // (LibreHardwareMonitor) starting at once crashes the new one.
        HANDLE old = parts.isEmpty() ? nullptr : OpenProcess(SYNCHRONIZE, FALSE, parts[0].toULong());
        if(old)
        {
            WaitForSingleObject(old, 15000);
            CloseHandle(old);
        }
        return parts.contains("min") ? ContinueHidden : Continue;
    }

    // A fresh hop file while still non-elevated means the task ran but didn't elevate (e.g. not an admin
    // account). Stay here instead of looping.
    if(QFileInfo::exists(hop) && QFileInfo(hop).lastModified().secsTo(QDateTime::currentDateTime()) < 20)
    {
        QFile::remove(hop);
        return Continue;
    }

    // Crash guard: if the last elevated start never reported healthy, it died (e.g. a plugin crashing
    // with admin rights). Don't hand off again, or OpenRGB would never come up. Delete elevate-attempt to retry.
    QString attempt = cfgDir + "/elevate-attempt";
    QString ok      = cfgDir + "/elevate-ok";
    if(QFileInfo::exists(attempt) && (!QFileInfo::exists(ok) || QFileInfo(ok).lastModified() < QFileInfo(attempt).lastModified()))
    {
        return Continue;
    }

    QString query = QString("schtasks.exe /query /tn \"%1\"").arg(QString::fromWCharArray(TASK_NAME));
    if(RunHidden(query) != 0)
    {
        if(QFileInfo::exists(declined)) return Continue;
        auto answer = QMessageBox::question(nullptr, "OpenRGB",
            "Some sensors (CPU temperatures in Hardware Sync) need OpenRGB to run as administrator.\n\n"
            "Set up OpenRGB to always start as administrator? Windows will ask once for permission.");
        if(answer != QMessageBox::Yes || !CreateTask(cfgDir))
        {
            QFile d(declined);
            d.open(QIODevice::WriteOnly);
            return Continue;
        }
    }

    QFile f(hop);
    if(f.open(QIODevice::WriteOnly))
    {
        f.write(QString("%1%2").arg(QCoreApplication::applicationPid())
                    .arg(QCoreApplication::arguments().contains("--startminimized") ? " min" : "").toUtf8());
        f.close();
    }
    QFile a(attempt);
    if(a.open(QIODevice::WriteOnly)) a.close();
    QString run = QString("schtasks.exe /run /tn \"%1\"").arg(QString::fromWCharArray(TASK_NAME));
    if(RunHidden(run) != 0)
    {
        QFile::remove(hop);
        return Continue;
    }
    return Quit;
}
