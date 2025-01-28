/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "joomlabackup.h"
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTextStream>

JoomlaBackup::JoomlaBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("Joomla"), QStringLiteral("configuration.php"), target, tempDir, options, parent)
{

}

JoomlaBackup::~JoomlaBackup() = default;

bool JoomlaBackup::loadConfiguration()
{
    QFile configFile(configFilePath());

    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString dbName, dbUser, dbPassword, dbType;
    QString dbHost = QStringLiteral("localhost");
    int dbPort = 3306; // NOLINT(cppcoreguidelines-avoid-magic-numbers)

    static QRegularExpression dbTypeRegEx(QStringLiteral("dbtype\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression dbNameRegEx(QStringLiteral("db\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression dbUserRegEx(QStringLiteral("user\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression dbPassRegEx(QStringLiteral("password\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression dbHostRegEx(QStringLiteral("host\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);

    bool dbTypeFound = false;
    bool dbNameFound = false;
    bool dbUserFound = false;
    bool dbPassFound = false;
    bool dbHostFound = false;

    QTextStream s(&configFile);
    QString line;
    while (s.readLineInto(&line)) {
        auto dbTypeMatch = dbTypeRegEx.match(line);
        if (dbTypeMatch.hasMatch()) {
            dbType = dbTypeMatch.captured(1).trimmed();
            dbTypeFound = true;
            continue;
        }
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

        if (dbTypeFound && dbNameFound && dbUserFound && dbPassFound && dbHostFound) {
            break;
        }
    }

    if (dbType.compare(QLatin1String("mysqli")) == 0 || dbType.compare(QLatin1String("pdomysql")) == 0) {
        setDbType(DbBackup::MySQL);
    } else  if (dbType.compare(QLatin1String("pgsql")) == 0 || dbType.compare(QLatin1String("postgresql")) == 0) {
        setDbType(DbBackup::PostgreSQL);
    } else {
        logError(qtTrId("SIHHURI_CRIT_INVALID_DB_TYPE").arg(dbType));
        return false;
    }

    setDbHost(dbHost);
    setDbPort(dbPort);
    setDbName(dbName);
    setDbUser(dbUser);
    setDbPassword(dbPassword);

    return true;
}

void JoomlaBackup::enableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_ENABLE_MAINTENANCE"));

    if (!changeMaintenance(true)) {
        logWarning(qtTrId("SIHHURI_WARN_FAILED_ENABLE_MAINTENANCE"));
    }
    doBackup();
}

void JoomlaBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &JoomlaBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &JoomlaBackup::onBackupDatabaseFailed);
    backupDatabase();
}

void JoomlaBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &JoomlaBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void JoomlaBackup::onBackupDatabaseFailed()
{
    disableMaintenance();
}

void JoomlaBackup::onBackupDirectoriesFinished()
{
    disableMaintenance();
}

void JoomlaBackup::disableMaintenance()
{
    logInfo(qtTrId("SIHHURI_INFO_DISABLE_MAINTENANCE"));

    if (!changeMaintenance(false)) {
        logWarning(qtTrId("SIHHURI_WARN_FAILED_DISABLE_MAINTENANCE"));
    }

    startTimer();
}

bool JoomlaBackup::changeMaintenance(bool activate)
{
    QFile configFile(configFilePath());

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString config = QString::fromUtf8(configFile.readAll());

    configFile.close();

    const QString replaceValue = activate ? QStringLiteral("$offline = '1'") : QStringLiteral("$offline = '0'");

    static QRegularExpression regex(QStringLiteral("\\$offline\\s*=\\s*[\"'][^\"']*[\"']"));
    config.replace(regex, replaceValue);

    if (!configFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    if (configFile.write(config.toUtf8()) < 0 ) {
        //% "Failed to write configuration file %1: %2"
        logError(qtTrId("SIHHURI_CRIT_FAILED_WRITE_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    configFile.close();

    return true;
}

#include "moc_joomlabackup.cpp"
