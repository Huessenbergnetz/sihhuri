/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ROUNDCUBEBACKUP_H
#define ROUNDCUBEBACKUP_H

#include "dbbackup.h"
#include <QObject>

class RoundcubeBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit RoundcubeBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~RoundcubeBackup() override;

protected:
    bool loadConfiguration() override;

    void doBackup() override;

private slots:
    void onBackupDatabaseFinished();
    void onBackupDatabaseFailed();
    void onBackupDirectoriesFinished();

private:
    Q_DISABLE_COPY(RoundcubeBackup)
};

#endif // ROUNDCUBEBACKUP_H
