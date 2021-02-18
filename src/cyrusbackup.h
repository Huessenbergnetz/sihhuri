/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CYRUSBACKUP_H
#define CYRUSBACKUP_H

#include "abstractbackup.h"
#include <QObject>

class CyrusBackup : public AbstractBackup
{
    Q_OBJECT
public:
    explicit CyrusBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~CyrusBackup() override;

protected:
    bool loadConfiguration() override;

    void doBackup() override;

private slots:
    void onBackupDirectoriesFinished();

private:
    QQueue<QString> m_secondRun;
    QString m_service;
    QString m_configDirectory;
    bool m_isFirstRun = true;

    void backupMailboxes();
    void stopService();
    void startService();

    Q_DISABLE_COPY(CyrusBackup)
};

#endif // CYRUSBACKUP_H
