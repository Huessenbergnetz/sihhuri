/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "roundcubebackup.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

RoundcubeBackup::RoundcubeBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : DbBackup(QStringLiteral("Roundcube"), QStringLiteral("config/config.inc.php"), target, tempDir, options, parent)
{

}

RoundcubeBackup::~RoundcubeBackup() = default;

bool RoundcubeBackup::loadConfiguration()
{
    QFile configFile(configFilePath());

    logInfo(qtTrId("SIHHURI_INFO_READING_CONFIG_FILE").arg(configFile.fileName()));

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        //% "Can not open configuration file %1: %2"
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_CONFIG_FILE").arg(configFile.fileName(), configFile.errorString()));
        return false;
    }

    int port = DbBackup::mysqlDefaultPort;

    QString dsn;

    static QRegularExpression dsnRegEx(QStringLiteral("\\$config\\[[\"']db_dsnw[\"']\\]\\s*=\\s*[\"']([^\"']*)[\"']"), QRegularExpression::CaseInsensitiveOption);

    QTextStream s(&configFile);
    QString line;
    while (s.readLineInto(&line)) {
        auto dsnMatch = dsnRegEx.match(line);
        if (dsnMatch.hasMatch()) {
            dsn = dsnMatch.captured(1).trimmed();
            break;
        }
    }

    const qsizetype leftFirstColonIdx = dsn.indexOf(QLatin1Char(':'));
    const QString scheme = dsn.left(leftFirstColonIdx);

    if (scheme.compare(QLatin1String("mysql"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::MySQL);
        port = DbBackup::mysqlDefaultPort;
    } else if (scheme.compare(QLatin1String("pgsql"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::PostgreSQL);
        port = DbBackup::pgsqlDefaultPort;
    } else if (scheme.compare(QLatin1String("sqlite"), Qt::CaseInsensitive) == 0) {
        setDbType(DbBackup::SQLite);
        return true;
    }

    const qsizetype lastAtIdx = dsn.lastIndexOf(QLatin1Char('@'));

    const QString authority = dsn.mid(leftFirstColonIdx + 3, lastAtIdx - leftFirstColonIdx - 3);
    const qsizetype authColonIdx = authority.indexOf(QLatin1Char(':'));
    const QString user = authority.left(authColonIdx);
    const QString password = authority.mid(authColonIdx + 1);

    const QString hostAndPath = dsn.mid(lastAtIdx + 1);
    const qsizetype slashIdx = hostAndPath.indexOf(QLatin1Char('/'));
    const QString hostAndPort = hostAndPath.left(slashIdx);
    const QString name = hostAndPath.mid(slashIdx + 1);
    const qsizetype portColonIdx = hostAndPort.indexOf(QLatin1Char(':'));
    QString host;
    if (portColonIdx > -1) {
        host = hostAndPort.left(portColonIdx);
        port = hostAndPort.mid(portColonIdx + 1).toInt();
    } else {
        host = hostAndPort;
    }

    setDbHost(host);
    setDbPort(port);
    setDbName(name);
    setDbUser(user);
    setDbPassword(password);

    return true;
}

void RoundcubeBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &RoundcubeBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &RoundcubeBackup::onBackupDatabaseFailed);
    backupDatabase();
}

void RoundcubeBackup::onBackupDatabaseFinished()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &RoundcubeBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void RoundcubeBackup::onBackupDatabaseFailed()
{
    emitFinished();
}

void RoundcubeBackup::onBackupDirectoriesFinished()
{
    emitFinished();
}

#include "moc_roundcubebackup.cpp"
