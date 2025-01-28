/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "giteabackup.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

GiteaBackup::GiteaBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("Gitea"), QStringLiteral("conf/app.ini"), target, tempDir, options, parent)
{

}

GiteaBackup::~GiteaBackup() = default;

bool GiteaBackup::loadConfiguration()
{
    m_service = option(QStringLiteral("service"), QStringLiteral("gitea")).toString();

    QFile configFile(configFilePath());

    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    QString dbType, dbName, dbUser, dbPassword;
    QString dbHost = QStringLiteral("localhost");
    int dbPort = DbBackup::mysqlDefaultPort;

    bool insideDbSection = false;
    bool dbTypeFound = false;
    bool dbNameFound = false;
    bool dbUserFound = false;
    bool dbPassFound = false;
    bool dbHostFound = false;

    QRegularExpression dbTypeRegEx(QStringLiteral("^DB_TYPE\\s*=\\s*(.*)$"));
    QRegularExpression dbNameRegEx(QStringLiteral("^NAME\\s*=\\s*(.*)$"));
    QRegularExpression dbUserRegEx(QStringLiteral("^USER\\s*=\\s*(.*)$"));
    QRegularExpression dbPassRegEx(QStringLiteral("^PASSWD\\s*=\\s*(.*)$"));
    QRegularExpression dbHostRegEx(QStringLiteral("^HOST\\s*=\\s*(.*)$"));

    QTextStream s(&configFile);
    QString line;
    while (s.readLineInto(&line)) {
        if (line.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (line.contains(QLatin1String("[database]"))) {
            insideDbSection = true;
            continue;
        }
        if (insideDbSection) {
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
        }
        if (dbTypeFound && dbNameFound && dbUserFound && dbPassFound && dbHostFound) {
            break;
        }
    }

    if (dbPassword.startsWith(QLatin1Char('`'))) {
        dbPassword.remove(0, 1);
        dbPassword.chop(1);
    }

    if (dbType.compare(QLatin1String("mysql"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::MySQL);
        dbPort = DbBackup::mysqlDefaultPort;
    } else if (dbType.compare(QLatin1String("postgres"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::PostgreSQL);
        dbPort = DbBackup::pgsqlDefaultPort;
    } else if (dbType.compare(QLatin1String("sqlite3"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::SQLite);
        return true;
    }

    if (!dbHost.startsWith(QLatin1Char('/')) && dbHost.contains(QLatin1Char(':'))) {
        const QStringList hostParts = dbHost.split(QLatin1Char(':'));
        if (hostParts.size() > 1) {
            setDbHost(hostParts.at(0));
            setDbPort(hostParts.at(1).toInt());
        } else {
            return false;
        }
    } else {
        setDbHost(dbHost);
        setDbPort(dbPort);
    }

    setDbName(dbName);
    setDbUser(dbUser);
    setDbPassword(dbPassword);

    return true;
}

void GiteaBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &GiteaBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &GiteaBackup::onBackupDatabaseFailed);
    stopService();
}

void GiteaBackup::stopService()
{
    auto systemctl = stopSystemdService(m_service);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        backupDatabase();
    });
    systemctl->start();
}

void GiteaBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &GiteaBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void GiteaBackup::onBackupDatabaseFailed()
{
    startService();
}

void GiteaBackup::onBackupDirectoriesFinished()
{
    startService();
}

void GiteaBackup::startService()
{
    auto systemctl = startSystemdService(m_service);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        disableMaintenance();
    });
    systemctl->start();
}

#include "moc_giteabackup.cpp"
