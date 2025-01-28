/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DBBACKUP_H
#define DBBACKUP_H

#include "abstractbackup.h"
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>

class DbBackup : public AbstractBackup
{
    Q_OBJECT
public:
    explicit DbBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~DbBackup() override;

protected:
    explicit DbBackup(const QString &type, const QString &configFile, const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);

    enum Type : quint8 {
        MySQL,
        MariaDB,
        PostgreSQL,
        SQLite,
        Invalid
    };

    bool loadConfiguration() override;

    void doBackup() override;

    void backupDatabase();

    void setDbType(Type type);
    [[nodiscard]] Type dbType() const;

    void setDbName(const QString &dbName);
    [[nodiscard]] QString dbName() const;

    void setDbUser(const QString &dbUser);
    [[nodiscard]] QString dbUser() const;

    void setDbPassword(const QString &dbPassword);
    [[nodiscard]] QString dbPassword() const;

    void setDbHost(const QString &dbHost);
    [[nodiscard]] QString dbHost() const;

    void setDbPort(int dbPort);
    [[nodiscard]] int dbPort() const;

    static const int mysqlDefaultPort;
    static const int pgsqlDefaultPort;

signals:
    void backupDatabaseFinished(QPrivateSignal);
    void backupDatabaseFailed(QPrivateSignal);

private slots:
    void onDatabaseDumpFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCompressDatabaseFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBackupDatabaseFinished();

private:
    QTemporaryFile m_dbConfigFile;
    qint64 m_fileSize = 0;
    QString m_backupFileName;
    QString m_dbName;
    QString m_dbUser;
    QString m_dbPass;
    QString m_dbHost;
    QString m_hashSum;
    QFile* m_dumpFile = nullptr;
    int m_dbPort = 0;
    Type m_type = Invalid;

    [[nodiscard]] QString dbDirPath() const;
    void backupMySql();
    void backupMariaDb();
    void backupPgSql();
    void hashDatabase();
    void compressDatabase();

    Q_DISABLE_COPY(DbBackup)
};

#endif // DBBACKUP_H
