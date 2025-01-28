/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nextcloudbackup.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QLocale>

NextcloudBackup::NextcloudBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("Nextcloud"), QStringLiteral("config/config.php"), target, tempDir, options, parent)
{

}

NextcloudBackup::~NextcloudBackup() = default;

bool NextcloudBackup::loadConfiguration()
{
    QFile configFile(configFilePath());

    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString dbName, dbUser, dbPassword, dbType;
    QString dbHost = QStringLiteral("localhost");
    int dbPort = DbBackup::mysqlDefaultPort;

    QRegularExpression dbTypeRegEx(QStringLiteral("[\"']dbtype[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbNameRegEx(QStringLiteral("[\"']dbname[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbUserRegEx(QStringLiteral("[\"']dbuser[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbPassRegEx(QStringLiteral("[\"']dbpassword[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbHostRegEx(QStringLiteral("[\"']dbhost[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression dbPortRegEx(QStringLiteral("[\"']dbport[\"']\\s*=>\\s*[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);

    bool dbTypeFound = false;
    bool dbNameFound = false;
    bool dbUserFound = false;
    bool dbPassFound = false;
    bool dbHostFound = false;
    bool dbPortFound = false;

    QTextStream s(&configFile);
    QString sLine;
    while (s.readLineInto(&sLine)) {
        auto dbTypeMatch = dbTypeRegEx.match(sLine);
        if (dbTypeMatch.hasMatch()) {
            dbType = dbTypeMatch.captured(1).trimmed();
            dbTypeFound = true;
            continue;
        }
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
        auto dbPortMatch = dbPortRegEx.match(sLine);
        if (dbPortMatch.hasMatch()) {
            dbPort = dbPortMatch.captured(1).trimmed().toInt();
            dbPortFound = true;
            continue;
        }

        if (dbTypeFound && dbNameFound && dbUserFound && dbPassFound && dbHostFound && dbPortFound) {
            break;
        }
    }

    if (dbType.compare(QLatin1String("sqlite3")) == 0) {
        setDbType(DbBackup::SQLite);
    } else if (dbType.compare(QLatin1String("mysql")) == 0) {
        setDbType(DbBackup::MySQL);
    } else if (dbType.compare(QLatin1String("pgsql")) == 0) {
        setDbType(DbBackup::PostgreSQL);
    } else {
        logError(qtTrId("SIHHURI_CRIT_INVALID_DB_TYPE").arg(dbType));
        return false;
    }

    setDbName(dbName);
    setDbUser(dbUser);
    setDbPassword(dbPassword);
    setDbHost(dbHost);
    setDbPort(dbPort);

    return true;
}

void NextcloudBackup::enableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_ENABLE_MAINTENANCE"));

    auto occ = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
    occ->setProgram(QStringLiteral("sudo"));
    occ->setWorkingDirectory(configFileRoot());
    occ->setArguments({QStringLiteral("-u"), user(), QStringLiteral("php"), QStringLiteral("./occ"), QStringLiteral("maintenance:mode"), QStringLiteral("--on")});
    connect(occ, &QProcess::readyReadStandardError, this, [this, occ](){
        logCritical(QStringLiteral("occ: %1").arg(QString::fromUtf8(occ->readAllStandardError())));
    });
    connect(occ, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            logWarning(qtTrId("SIHHURI_WARN_FAILED_ENABLE_MAINTENANCE"));
        }
        doBackup();
    });
    occ->start();
}

void NextcloudBackup::doBackup()
{
    if (dbType() != DbBackup::SQLite) {
        connect(this, &DbBackup::backupDatabaseFinished, this, &NextcloudBackup::onBackupDatabaseFinished);
        connect(this, &DbBackup::backupDatabaseFailed, this, &NextcloudBackup::onBackupDatabaseFailed);
        backupDatabase();
    } else {
        backupDirectories();
    }
}

void NextcloudBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &NextcloudBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void NextcloudBackup::onBackupDatabaseFailed()
{
    disableMaintenance();
}

void NextcloudBackup::onBackupDirectoriesFinished()
{
    disableMaintenance();
}

void NextcloudBackup::disableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_DISABLE_MAINTENANCE"));

    auto occ = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
    occ->setProgram(QStringLiteral("sudo"));
    occ->setWorkingDirectory(configFileRoot());
    occ->setArguments({QStringLiteral("-u"), user(), QStringLiteral("php"), QStringLiteral("./occ"), QStringLiteral("maintenance:mode"), QStringLiteral("--off")});
    connect(occ, &QProcess::readyReadStandardError, this, [this, occ](){
        logCritical(QStringLiteral("occ: %1").arg(QString::fromUtf8(occ->readAllStandardError())));
    });
    connect(occ, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            logWarning(qtTrId("SIHHURI_WARN_FAILED_DISABLE_MAINTENANCE"));
        }
        startTimer();
    });
    occ->start();
}

#include "moc_nextcloudbackup.cpp"
