/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QObject>
#include <QVariantMap>
#include <QTemporaryDir>
#include <QQueue>
#include <QProcess>
#include <QFileInfo>
#include <chrono>

class AbstractBackup;

class BackupManager : public QObject
{
    Q_OBJECT
public:
    explicit BackupManager(const QVariantMap &config, const QStringList &types, QObject *parent = nullptr);
    ~BackupManager() override;

    void start();

private slots:
    void doStart();
    void runBackup();
    void onTarDbsFinished();
    void doChangeOwner();

private:
    void tarDbs();
    void changeOwner();
    void handleError(const QString &msg, int exitCode);

    QVariantMap m_config;
    QStringList m_types;
    QString m_depot;
    QString m_owner;
    QTemporaryDir m_tempDir;
    QQueue<AbstractBackup*> m_items;
    QQueue<QFileInfo> m_changeOwnerQueue;
    AbstractBackup* m_currentItem = nullptr;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_timeStart;
    int m_enabledItemsSize = 0;

    Q_DISABLE_COPY(BackupManager)
};

#endif // BACKUPMANAGER_H
