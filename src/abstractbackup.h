/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ABSTRACTBACKUP_H
#define ABSTRACTBACKUP_H

#include <QObject>
#include <QVariantMap>
#include <QQueue>
#include <chrono>
#include <utility>
#include <vector>

class QProcess;

struct BackupStats {
    enum Type : quint8 {
        Undefined,
        Directory,
        MySQL,
        PostgreSQL
    };

    Type type = Undefined;
    QString id;
    qint64 filesBefore = 0;
    qint64 sizeBefore = 0;
    qint64 filesAfter = 0;
    qint64 sizeAfter = 0;
    qint64 uncompressedSize = 0;
    qint64 compressedSize = 0;
    qint64 timeUsed = 0;
};

class AbstractBackup : public QObject
{
    Q_OBJECT
public:
    explicit AbstractBackup(const QString &type, const QString &configFile, const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent = nullptr);

    ~AbstractBackup() override;

    void start();

    QStringList errors() const;
    QStringList warnings() const;
    QString id() const;
    std::vector<BackupStats> statistics() const;

protected:
    virtual bool loadConfiguration() = 0;

    virtual void enableMaintenance();
    virtual void disableMaintenance();

    virtual void doBackup();

    void backupDirectories();

    void logInfo(const QString &msg) const;
    void logWarning(const QString &msg);
    void logCritical(const QString &msg) const;
    void logError(const QString &msg);
    QString logMsg(const QString &msg) const;

    void setStartTime();
    qint64 getTimeUsed() const;

    void setStepStartTime();
    qint64 getStepTimeUsed() const;

    std::pair<qint64,qint64> getDirSize(const QString &path) const;

    QVariant option(const QString &key, const QVariant &defValue = QVariant()) const;
    QString target() const;
    QString tempDir() const;
    QString configFilePath() const;
    QString configFileRoot() const;
    QString user() const;
    QQueue<QString> directoryQueue() const;
    void setDirectoryQueue(const QQueue<QString> &queue);

    void emitFinished();

    void stopTimer();
    void startTimer();

    void addStatistic(const BackupStats &statistic);

    QProcess* startStopServiceOrTimer(const QString &unit, bool stop = false);

    BackupStats m_currentStats;

signals:
    void backupDirectoriesFinished(QPrivateSignal);
    void finished(QPrivateSignal);

private slots:
    void startBackup();
    void isTimerServiceActive();

private:
    QVariantMap m_options;
    QString m_type;
    QString m_configFileName;
    QString m_configFilePath;
    QString m_configFileRoot;
    QString m_target;
    QString m_tempDir;
    QStringList m_errors;
    QStringList m_warnings;
    QString m_user;
    QString m_timer;
    QQueue<QString> m_dirQueue;
    std::vector<BackupStats> m_stats;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_timeStart;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_stepTimeStart;
    int m_maxTryIsServiceActive = 30;
    int m_tryCountIsServiceActive = 0;
    int m_waitSecondsIsServiceActive = 10;

    Q_DISABLE_COPY(AbstractBackup)
};

#endif // ABSTRACTBACKUP_H
