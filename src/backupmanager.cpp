/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "backupmanager.h"
#include "directorybackup.h"
#include "dbbackup.h"
#include "wordpressbackup.h"
#include "nextcloudbackup.h"
#include "joomlabackup.h"
#include "matomobackup.h"
#include "cyrusbackup.h"
#include "roundcubebackup.h"
#include "giteabackup.h"
#include <QTimer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDate>
#include <QLocalServer>
#include <QStandardPaths>

BackupManager::BackupManager(const QVariantMap &config, const QStringList &types, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_types(types)
{

}

BackupManager::~BackupManager() = default;

void BackupManager::start()
{
    QTimer::singleShot(0, this, &BackupManager::doStart);
}

void BackupManager::doStart()
{
    m_timeStart = std::chrono::high_resolution_clock::now();

    const QVariantMap globalConfig = m_config.value(QStringLiteral("global")).toMap();
    m_depot = globalConfig.value(QStringLiteral("depot")).toString();
    m_owner = globalConfig.value(QStringLiteral("owner"), QStringLiteral("root")).toString();

    QFileInfo depotFi(m_depot);
    if (Q_UNLIKELY(!depotFi.exists() || !depotFi.isDir())) {
        //% "Can not find depot directory at %1."
        handleError(qtTrId("SIHHURI_CRIT_DEPOT_NOT_FOUND").arg(m_depot), 1);
        return;
    }

    const QVariantList items = m_config.value(QStringLiteral("items")).toList();
    if (Q_UNLIKELY(items.empty())) {
        //% "No backup items have been configured."
        handleError(qtTrId("SIHHURI_CRIT_NO_BACKUP_ITEMS_CONFIGURED"), 6);
        return;
    }

    const QString xzExecPath = QStandardPaths::findExecutable(QStringLiteral("xz"));
    if (Q_UNLIKELY(xzExecPath.isEmpty())) {
        //% "Can not find xz executable to compress database dump files."
        handleError(qtTrId("SIHHURI_CRIT_NO_PIXZ_EXECUTABE"), 6);
        return;
    }

    for (const QVariant &item : items) {
        const QVariantMap o = item.toMap();
        if (o.value(QStringLiteral("enabled"), true).toBool()) {
            const QString type = o.value(QStringLiteral("type")).toString();
            if (!m_types.empty() && !m_types.contains(type, Qt::CaseInsensitive)) {
                continue;
            }
            if (type.compare(QLatin1String("directory"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new DirectoryBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("mariadb"), Qt::CaseInsensitive) == 0 || type.compare(QLatin1String("mysql"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new DbBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("wordpress"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new WordPressBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("nextcloud"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new NextcloudBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("joomla"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new JoomlaBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("matomo"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new MatomoBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("cyrus"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new CyrusBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("roundcube"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new RoundcubeBackup(m_depot, m_tempDir.path(), o, this));
            } else if (type.compare(QLatin1String("gitea"), Qt::CaseInsensitive) == 0) {
                m_items.enqueue(new GiteaBackup(m_depot, m_tempDir.path(), o, this));
            } else {
                //% "%1 is not a valid backup item type. Omitting this entry."
                qWarning("%s", qUtf8Printable(qtTrId("SIHHURI_WARN_INVALID_ITEM_TYPE").arg(type)));
            }
        }
    }

    if (Q_UNLIKELY(m_items.empty())) {
        //% "No backup items are available."
        handleError(qtTrId("SIHHURI_CRIT_NO_BACK_ITEMS_AVAILABLE"), 6);
        return;
    }

    m_enabledItemsSize = m_items.size();
    //% "Starting backup of %n items."
    qInfo("%s", qUtf8Printable(qtTrId("SIHHURI_INFO_BACKUPMANAGER_START", m_enabledItemsSize)));

    runBackup();
}

void BackupManager::runBackup()
{
    if (m_currentItem) {
        const QStringList errors = m_currentItem->errors();
        if (!errors.empty()) {
            m_errors.push_back(std::make_pair(m_currentItem->id(), errors));
        }
        const QStringList warnings = m_currentItem->warnings();
        if (!warnings.empty()) {
            m_warnings.push_back(std::make_pair(m_currentItem->id(), warnings));
        }

        const std::vector<BackupStats> stats = m_currentItem->statistics();
        for (const BackupStats &s : stats) {
            m_stats.push_back(s);
        }

        m_currentItem->deleteLater();
        m_currentItem = nullptr;
    }

    if (m_items.empty()) {
        changeOwner();
        return;
    }

    m_currentItem = m_items.dequeue();
    connect(m_currentItem, &AbstractBackup::finished, this, &BackupManager::runBackup);
    m_currentItem->start();
}

void BackupManager::changeOwner()
{
    QDir depotDir(m_depot);
    const QFileInfoList depotDirFis = depotDir.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : depotDirFis) {
        m_changeOwnerQueue.enqueue(fi);
    }

    doChangeOwner();
}

void BackupManager::doChangeOwner()
{
    if (m_changeOwnerQueue.empty()) {
        finish();
        return;
    }

    const QFileInfo fi = m_changeOwnerQueue.dequeue();

    if (fi.fileName() != QLatin1String("lost+found")) {
        auto chown = new QProcess(this);
        chown->setProgram(QStringLiteral("chown"));
        chown->setWorkingDirectory(m_depot);
        QStringList args;
        if (fi.isDir()) {
            args << QStringLiteral("-R");
        }
        args << m_owner;
        args << fi.fileName();
        chown->setArguments(args);
        connect(chown, &QProcess::readyReadStandardError, this, [=](){
            qWarning("chown: %s", chown->readAllStandardError().constData());
        });
        connect(chown, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
            doChangeOwner();
        });
        chown->start();
    } else {
        doChangeOwner();
    }
}

void BackupManager::finish()
{
    const auto timeEnd = std::chrono::high_resolution_clock::now();
    const auto timeUsed = static_cast<qint32>(std::chrono::duration_cast<std::chrono::seconds>(timeEnd - m_timeStart).count());

    int errorCount = 0;
    int warningCount = 0;

    if (!m_errors.empty()) {
        for (const std::pair<QString,QStringList> &itemErrors : std::as_const(m_errors)) {
            errorCount += itemErrors.second.size();
        }
    }

    if (!m_warnings.empty()) {
        for (const std::pair<QString,QStringList> &itemWarnings : std::as_const(m_warnings)) {
            warningCount += itemWarnings.second.size();
        }
    }

    qint64 files = 0;
    qint64 size = 0;

    for (const BackupStats &stats : m_stats) {
        files += stats.filesAfter;
        size += stats.sizeAfter;
        size += stats.compressedSize;
    }

    QLocale locale;

    //% "Finished backup of %1 items in %2 seconds. Errors: %3, Warnings: %4, Files: %5, Size: %6"
    const QString msg = qtTrId("SIHHURI_INFO_FINISHED_COMPLETE_BACKUP").arg(QString::number(m_enabledItemsSize), QString::number(timeUsed), QString::number(errorCount), QString::number(warningCount), locale.toString(files), locale.formattedDataSize(size));
    if (errorCount > 0) {
        qCritical("%s", qUtf8Printable(msg));
    } else if (warningCount > 0) {
        qWarning("%s", qUtf8Printable(msg));
    } else {
        qInfo("%s", qUtf8Printable(msg));
    }
    QCoreApplication::exit();
}

void BackupManager::handleError(const QString &msg, int exitCode)
{
    qCritical("%s", qUtf8Printable(msg));

    QCoreApplication::exit(exitCode);
}

#include "moc_backupmanager.cpp"
