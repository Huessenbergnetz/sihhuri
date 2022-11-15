/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MATOMOBACKUP_H
#define MATOMOBACKUP_H

#include "dbbackup.h"
#include <QObject>

class MatomoBackup final : public DbBackup
{
    Q_OBJECT
public:
    explicit MatomoBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~MatomoBackup() final;

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
    Q_DISABLE_COPY(MatomoBackup)
};

#endif // MATOMOBACKUP_H
