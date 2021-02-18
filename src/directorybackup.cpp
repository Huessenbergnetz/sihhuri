/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directorybackup.h"

DirectoryBackup::DirectoryBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : AbstractBackup(QStringLiteral("Directory"), QString(), target, tempDir, options, parent)
{

}

DirectoryBackup::~DirectoryBackup() = default;

bool DirectoryBackup::loadConfiguration()
{
    return true;
}

void DirectoryBackup::doBackup()
{
    connect(this, &AbstractBackup::backupDirectoriesFinished, this, &DirectoryBackup::onBackupDirectoriesFinished);
    backupDirectories();
}

void DirectoryBackup::onBackupDirectoriesFinished()
{
    disableMaintenance();
}

#include "moc_directorybackup.cpp"
