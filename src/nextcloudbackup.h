/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NEXTCLOUDBACKUP_H
#define NEXTCLOUDBACKUP_H

#include "dbbackup.h"
#include <QObject>
#include <QQueue>

class NextcloudBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit NextcloudBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~NextcloudBackup() override;

protected:
    bool loadConfiguration() override;

    void doBackup() override;

    void enableMaintenance() override;
    void disableMaintenance() override;

private slots:
    void onBackupDatabaseFinished();
    void onBackupDatabaseFailed();
    void onBackupDirectoriesFinished();

private:
    Q_DISABLE_COPY(NextcloudBackup)
};

#endif // NEXTCLOUDBACKUP_H
