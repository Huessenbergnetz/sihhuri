/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef JOOMLABACKUP_H
#define JOOMLABACKUP_H

#include "dbbackup.h"
#include <QObject>

class JoomlaBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit JoomlaBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~JoomlaBackup() override;

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
    bool changeMaintenance(bool activate);

    Q_DISABLE_COPY(JoomlaBackup)
};

#endif // JOOMLABACKUP_H
