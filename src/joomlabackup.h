/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef JOOMLABACKUP_H
#define JOOMLABACKUP_H

#include "dbbackup.h"
#include <QObject>

class JoomlaBackup final : public DbBackup
{
    Q_OBJECT
public:
    explicit JoomlaBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~JoomlaBackup() final;

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
    bool changeMaintenance(bool activate);

    Q_DISABLE_COPY(JoomlaBackup)
};

#endif // JOOMLABACKUP_H
