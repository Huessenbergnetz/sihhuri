/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "matomobackup.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTextStream>

MatomoBackup::MatomoBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("Matomo"), QStringLiteral("config/config.ini.php"), target, tempDir, options, parent)
{

}

MatomoBackup::~MatomoBackup() = default;

bool MatomoBackup::loadConfiguration()
{
    QFile configFile(configFilePath());

    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        //% "Can not open configuration file %1: %2"
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString dbName, dbUser, dbPassword;
    QString dbHost = QStringLiteral("localhost");
    int dbPort = 3306;

    QRegularExpression dbNameRegEx(QStringLiteral("\\s*dbname\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbUserRegEx(QStringLiteral("\\s*username\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbPassRegEx(QStringLiteral("\\s*password\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbHostRegEx(QStringLiteral("\\s*host\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);

    bool dbNameFound = false;
    bool dbUserFound = false;
    bool dbPassFound = false;
    bool dbHostFound = false;

    bool inDatabaseSection = false;

    QTextStream s;
    s.setDevice(&configFile);
    QString line;
    while (s.readLineInto(&line)) {
        if (line.contains(QLatin1String("[database]"))) {
            inDatabaseSection = true;
        }

        if (inDatabaseSection) {
            auto dbNameMatch = dbNameRegEx.match(line);
            if (dbNameMatch.hasMatch()) {
                dbName = dbNameMatch.captured(1).trimmed();
                dbNameFound = true;
                continue;
            }
            auto dbUserMatch = dbUserRegEx.match(line);
            if (dbUserMatch.hasMatch()) {
                dbUser = dbUserMatch.captured(1).trimmed();
                dbUserFound = true;
                continue;
            }
            auto dbPassMatch = dbPassRegEx.match(line);
            if (dbPassMatch.hasMatch()) {
                dbPassword = dbPassMatch.captured(1).trimmed();
                dbPassFound = true;
                continue;
            }
            auto dbHostMatch = dbHostRegEx.match(line);
            if (dbHostMatch.hasMatch()) {
                dbHost = dbHostMatch.captured(1).trimmed();
                dbHostFound = true;
                continue;
            }
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

void MatomoBackup::enableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_ENABLE_MAINTENANCE"));

    QStringList args({QStringLiteral("-u"), user(), QStringLiteral("php"), QStringLiteral("console"), QStringLiteral("config:set"), QStringLiteral("General.maintenance_mode=1"), QStringLiteral("Tracker.record_statistics=0")});

    auto matomo = new QProcess(this);
    matomo->setWorkingDirectory(configFileRoot());
    matomo->setArguments(args);
    matomo->setProgram(QStringLiteral("sudo"));
    connect(matomo, &QProcess::readyReadStandardError, this, [=](){
        logCritical(QStringLiteral("console: %1").arg(QString::fromUtf8(matomo->readAllStandardError())));
    });
    connect(matomo, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            logWarning(qtTrId("SIHHURI_WARN_FAILED_ENABLE_MAINTENANCE"));
        }
        doBackup();
    });
    matomo->start();
}

void MatomoBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &MatomoBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &MatomoBackup::onBackupDatabaseFailed);
    backupDatabase();
}

void MatomoBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &MatomoBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void MatomoBackup::onBackupDatabaseFailed()
{
    disableMaintenance();
}

void MatomoBackup::onBackupDirectoriesFinished()
{
    disableMaintenance();
}

void MatomoBackup::disableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_DISABLE_MAINTENANCE"));

    QStringList args({QStringLiteral("-u"), user(), QStringLiteral("php"), QStringLiteral("console"), QStringLiteral("config:set"), QStringLiteral("General.maintenance_mode=0"), QStringLiteral("Tracker.record_statistics=1")});

    auto matomo = new QProcess(this);
    matomo->setWorkingDirectory(configFileRoot());
    matomo->setArguments(args);
    matomo->setProgram(QStringLiteral("sudo"));
    connect(matomo, &QProcess::readyReadStandardError, this, [=](){
        logCritical(QStringLiteral("console: %1").arg(QString::fromUtf8(matomo->readAllStandardError())));
    });
    connect(matomo, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            logWarning(qtTrId("SIHHURI_WARN_FAILED_DISABLE_MAINTENANCE"));
        }
        startTimer();
    });
    matomo->start();
}

#include "moc_matomobackup.cpp"
