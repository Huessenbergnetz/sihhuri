/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NEXTCLOUDBACKUP_H
#define NEXTCLOUDBACKUP_H

#include "dbbackup.h"
#include <QObject>
#include <QQueue>

class NextcloudBackup final : public DbBackup
{
    Q_OBJECT
public:
    explicit NextcloudBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~NextcloudBackup() final;

protected:
    bool loadConfiguration() final;

    void doBackup() final;

    void enableMaintenance() final;
    void disableMaintenance() final;

private slots:
    void onBackupDatabaseFinished();
    void onBackupDatabaseFailed();
    void onBackupDirectoriesFinished();

private:
    Q_DISABLE_COPY(NextcloudBackup)
};

#endif // NEXTCLOUDBACKUP_H
