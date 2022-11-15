/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cyrusbackup.h"
#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QLocale>
#include <QTimer>

CyrusBackup::CyrusBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : AbstractBackup(QStringLiteral("Cyrus"), QString(), target, tempDir, options, parent)
{

}

CyrusBackup::~CyrusBackup() = default;

bool CyrusBackup::loadConfiguration()
{
    m_service = option(QStringLiteral("service"), QStringLiteral("cyrus-imapd")).toString();

    // find configdirectory
    const QStringList dirs = option(QStringLiteral("directories")).toStringList();
    for (const QString &dir : dirs) {
        QFileInfo mailboxesdbFi(dir + QLatin1String("/mailboxes.db"));
        if (mailboxesdbFi.exists()) {
            m_configDirectory = dir;
            break;
        }
    }

    return true;
}

void CyrusBackup::doBackup()
{
    m_secondRun = directoryQueue();
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &CyrusBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void CyrusBackup::onBackupDirectoriesFinished()
{
    if (m_isFirstRun) {
        m_isFirstRun = false;
        setDirectoryQueue(m_secondRun);
        backupMailboxes();
    } else {
        startService();
    }
}

void CyrusBackup::backupMailboxes()
{
    if (m_configDirectory.isEmpty()) {
        //% "Can not find Cyrus configuration directory, omitting dumping of mailboxes database."
        logWarning(qtTrId("SIHHURI_WARN_CYRUS_CONF_DIR_NOT_FOUND"));
        stopService();
        return;
    }

    QString ctl_mboxlistPath = QStandardPaths::findExecutable(QStringLiteral("ctl_mboxlist"));

    if (ctl_mboxlistPath.isEmpty()) {
        ctl_mboxlistPath = QStandardPaths::findExecutable(QStringLiteral("ctl_mboxlist"), {QStringLiteral("/usr/lib/cyrus"), QStringLiteral("/usr/lib/cyrus/bin"), QStringLiteral("/usr/libexec/cyrus"), QStringLiteral("/usr/libexec/cyrus/bin")});
    }

    if (ctl_mboxlistPath.isEmpty()) {
        //% "Can not find ctl_mboxlist executable, omitting dumping of mailboxes database."
        logWarning(qtTrId("SIHHURI_WARN_CYRUS_CTL_MBOXLIST_NOT_FOUND"));
        stopService();
        return;
    }

    auto dbDumpFile = new QFile(m_configDirectory + QLatin1String("/mailboxlist.txt"), this);
    if (!dbDumpFile->open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) {
        //% "Can not open %1 to dump mailboxes databas: %2. Omitting dumping of mailboxes database."
        logWarning(qtTrId("SIHHURI_WARN_CYRUS_OPEN_CYRUS_DBDUMPFILE").arg(dbDumpFile->fileName(), dbDumpFile->errorString()));
        stopService();
        return;
    }

    setStepStartTime();
    //% "Starting dump of mailboxes database."
    logInfo(qtTrId("SIHHURI_INFO_START_MBOXLIST_DUMP"));

    auto ctl_mboxlist = new QProcess(this);
    ctl_mboxlist->setWorkingDirectory(m_configDirectory);
    ctl_mboxlist->setProgram(ctl_mboxlistPath);
    ctl_mboxlist->setArguments({QStringLiteral("-d")});
    connect(ctl_mboxlist, &QProcess::readyReadStandardError, this, [=](){
        logWarning(QStringLiteral("ctl_mboxlist: %1").arg(QString::fromUtf8(ctl_mboxlist->readAllStandardError())));
    });
    connect(ctl_mboxlist, &QProcess::readyReadStandardOutput, this, [=]{
        if (dbDumpFile->write(ctl_mboxlist->readAllStandardOutput()) < 0) {
            //% "Can not write mailboxes database to %1: %2"
            logWarning(qtTrId("SIHHURI_WARN_CYRUS_FAILED_WRITE_DBDUMPFILE").arg(dbDumpFile->fileName(), dbDumpFile->errorString()));
        }
    });
    connect(ctl_mboxlist, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        const qint64 timeUsed = getStepTimeUsed();
        QLocale locale;
        dbDumpFile->close();
        QFileInfo fi(dbDumpFile->fileName());
        //% "Finished dump of mailboxes database with %1 in %2 milliseconds."
        logInfo(qtTrId("SIHHURI_INFO_FINISHED_MBOXLIST_DUMP").arg(locale.formattedDataSize(fi.size()), locale.toString(timeUsed)));
        stopService();
    });
    ctl_mboxlist->start();
}

void CyrusBackup::stopService()
{
    auto systemctl = stopSystemdService(m_service);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        const int waitSeconds = 10;
        //% "Waiting %1 seconds for Cyrus to completely shut down."
        logInfo(qtTrId("SIHHURI_INFO_WAIT_CYRUS_SHUTDOWN").arg(waitSeconds));
        QTimer::singleShot(waitSeconds * 1000, this, [=](){
            backupDirectories();
        });
    });
    systemctl->start();
}

void CyrusBackup::startService()
{
    auto systemctl = startSystemdService(m_service);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        disableMaintenance();
    });
    systemctl->start();
}

#include "moc_cyrusbackup.cpp"
