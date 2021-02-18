/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GITEABACKUP_H
#define GITEABACKUP_H

#include "dbbackup.h"
#include <QObject>

class GiteaBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit GiteaBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~GiteaBackup() override;

protected:
    bool loadConfiguration() override;

    void doBackup() override;

private slots:
    void onBackupDatabaseFinished();
    void onBackupDatabaseFailed();
    void onBackupDirectoriesFinished();

private:
    QString m_service;

    void stopService();
    void startService();

    Q_DISABLE_COPY(GiteaBackup)
};

#endif // GITEABACKUP_H
