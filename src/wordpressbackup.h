/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WORDPRESSBACKUP_H
#define WORDPRESSBACKUP_H

#include "dbbackup.h"
#include <QObject>

class WordPressBackup : public DbBackup
{
    Q_OBJECT
public:
    explicit WordPressBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~WordPressBackup() override;

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
    Q_DISABLE_COPY(WordPressBackup)
};

#endif // WORDPRESSBACKUP_H
