/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MATOMOBACKUP_H
#define MATOMOBACKUP_H

#include "dbbackup.h"
#include <QObject>

class MatomoBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit MatomoBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~MatomoBackup() override;

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
    Q_DISABLE_COPY(MatomoBackup)
};

#endif // MATOMOBACKUP_H
