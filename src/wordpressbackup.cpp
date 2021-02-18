/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wordpressbackup.h"
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDir>
#include <QCryptographicHash>

WordPressBackup::WordPressBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("WordPress"), QStringLiteral("wp-config.php"), target, tempDir, options, parent)
{

}

WordPressBackup::~WordPressBackup() = default;

bool WordPressBackup::loadConfiguration()
{
    QFile configFile(configFilePath());

    //% "Reading configuration file %1."
    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        //% "Can not open configuration file %1: %2"
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString dbName, dbUser, dbPassword;
    QString dbHost = QStringLiteral("localhost");
    int dbPort = 3306;

    QRegularExpression dbNameRegEx(QStringLiteral("define\\s*\\(\\s*[\"']DB_NAME[\"']\\s*,\\s*[\"']([^\"']+)[\"']\\s*\\)\\s*;"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbUserRegEx(QStringLiteral("define\\s*\\(\\s*[\"']DB_USER[\"']\\s*,\\s*[\"']([^\"']+)[\"']\\s*\\)\\s*;"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbPassRegEx(QStringLiteral("define\\s*\\(\\s*[\"']DB_PASSWORD[\"']\\s*,\\s*[\"']([^\"']+)[\"']\\s*\\)\\s*;"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbHostRegEx(QStringLiteral("define\\s*\\(\\s*[\"']DB_HOST[\"']\\s*,\\s*[\"']([^\"']+)[\"']\\s*\\)\\s*;"), QRegularExpression::CaseInsensitiveOption);

    bool dbNameFound = false;
    bool dbUserFound = false;
    bool dbPassFound = false;
    bool dbHostFound = false;

    QTextStream s(&configFile);
    QString sLine;
    while (s.readLineInto(&sLine)) {
        auto dbNameMatch = dbNameRegEx.match(sLine);
        if (dbNameMatch.hasMatch()) {
            dbName = dbNameMatch.captured(1).trimmed();
            dbNameFound = true;
            continue;
        }
        auto dbUserMatch = dbUserRegEx.match(sLine);
        if (dbUserMatch.hasMatch()) {
            dbUser = dbUserMatch.captured(1).trimmed();
            dbUserFound = true;
            continue;
        }
        auto dbPassMatch = dbPassRegEx.match(sLine);
        if (dbPassMatch.hasMatch()) {
            dbPassword = dbPassMatch.captured(1).trimmed();
            dbPassFound = true;
            continue;
        }
        auto dbHostMatch = dbHostRegEx.match(sLine);
        if (dbHostMatch.hasMatch()) {
            dbHost = dbHostMatch.captured(1).trimmed();
            dbHostFound = true;
            continue;
        }

        if (dbNameFound && dbUserFound && dbPassFound && dbHostFound) {
            break;
        }
    }

    setDbType(DbBackup::MySQL);
    setDbHost(dbHost);
    setDbPort(dbPort);
    setDbName(dbName);
    setDbUser(dbUser);
    setDbPassword(dbPassword);

    return true;
}


void WordPressBackup::enableMaintenance()
{
    //% "Enabling maintenance mode."
    logInfo(qtTrId("SIHHURI_INFO_ENABLE_MAINTENANCE"));

    QStringList args({QStringLiteral("-u"), user(), QStringLiteral("wp"), QStringLiteral("maintenance-mode"), QStringLiteral("activate")});

    auto wpMaintenanceProc = new QProcess(this);
    wpMaintenanceProc->setWorkingDirectory(configFileRoot());
    wpMaintenanceProc->setArguments(args);
    wpMaintenanceProc->setProgram(QStringLiteral("sudo"));
    connect(wpMaintenanceProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            //% "Failed to enable maintenance mode."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_ENABLE_MAINTENANCE"));
        }
        doBackup();
    });
    wpMaintenanceProc->start();
}

void WordPressBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &WordPressBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &WordPressBackup::onBackupDatabaseFailed);
    backupDatabase();
}

void WordPressBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &WordPressBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void WordPressBackup::onBackupDatabaseFailed()
{
    disableMaintenance();
}

void WordPressBackup::onBackupDirectoriesFinished()
{
    disableMaintenance();
}

void WordPressBackup::disableMaintenance()
{
    //% "Disabling maintenance mode."
    logInfo(qtTrId("SIHHURI_INFO_DISABLE_MAINTENANCE"));

    QStringList args({QStringLiteral("-u"), user(), QStringLiteral("wp"), QStringLiteral("maintenance-mode"), QStringLiteral("deactivate")});

    auto wpMaintenanceProc = new QProcess(this);
    wpMaintenanceProc->setWorkingDirectory(configFileRoot());
    wpMaintenanceProc->setArguments(args);
    wpMaintenanceProc->setProgram(QStringLiteral("sudo"));
    connect(wpMaintenanceProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            //% "Failed to disable maintenance mode."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_DISABLE_MAINTENANCE"));
        }
        startTimer();
    });
    wpMaintenanceProc->start();
}

#include "moc_wordpressbackup.cpp"
