/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTORYBACKUP_H
#define DIRECTORYBACKUP_H

#include "abstractbackup.h"
#include <QObject>

class DirectoryBackup : public AbstractBackup
{
    Q_OBJECT
public:
    explicit DirectoryBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);
    ~DirectoryBackup() override;

protected:
    bool loadConfiguration() override;

    void doBackup() override;

private slots:
    void onBackupDirectoriesFinished();

private:
    Q_DISABLE_COPY(DirectoryBackup)
};

#endif // DIRECTORYBACKUP_H
