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

    void logDebug(const QString &msg) const;
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

    /*!
     * \brief Starts or stops systemd units.
     * \param unit  Name of the systemd service or timer unit name.
     * \param stop  Set this to \c true to stop the unit.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to start or stop systemd timer or service units and returns
     * a pointer to the created object. The creating object will be the parent. \a unit has to
     * be the unit file name like \c gitea.service or \c wp-cron.timer.
     *
     * \par Example
     * \code{.cpp}
     * auto service = startStopServiceOrTimer("gitea.service", false);
     * connect(service, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int exitCode, QProcess::ExitStatus exitStatus){
     *     if (exitCode != 0 && exitStatus != QProcess::NormalExit) {
     *         // do stuff
     *     } else {
     *         // do other stuff
     *     }
     * });
     * service->start()
     * \endcode
     */
    QProcess* startStopServiceOrTimer(const QString &unit, bool stop = false);
    /*!
     * \brief Starts a systemd unit.
     * \param unit  Name of the systemd service or timer unit.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to start a systemd timer or service unit and returns a pointer
     * to the created object. The creating object will be the parent. \a unit has to be the
     * unit file name like \c gitea.service or \c wp-cron.timer.
     *
     * \sa startStopServiceOrTimer(), stopServiceOrTimer()
     */
    QProcess* startServiceOrTimer(const QString &unit);
    /*!
     * \brief Stops a systemd unit.
     * \param unit  Name of the systemd service or timer unit.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to stop a systemd timer or service unit and returns a pointer
     * to the created object. The creating object will be the parent. \a unit has to be the
     * unit file name like \c gitea.service or \c wp-cron.timer.
     *
     * \sa startStopServiceOrTimer(), startServiceOrTimer()
     */
    QProcess* stopServiceOrTimer(const QString &unit);
    /*!
     * \brief Starts a systemd service unit.
     * \param unit  Name of the systemd service without extension.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to start a systemd service unit and returns a pointer to
     * the created object. The creating object will be the parent. \a unit has to be the unit
     * file name without extension like \c gitea. The extension \c .service will automatically
     * be added.
     *
     * \sa startStopServiceOrTimer(), stopService()
     */
    QProcess* startService(const QString &unit);
    /*!
     * \brief Stops a systemd service unit.
     * \param unit  Name of the systemd service without extension.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to stop a systemd service unit and returns a pointer to
     * the created object. The creating object will be the parent. \a unit has to be the unit
     * file name without extension like \c gitea. The extension \c .service will automatically
     * be added.
     *
     * \sa startStopServiceOrTimer(), startService()
     */
    QProcess* stopService(const QString &unit);
    /*!
     * \brief Starts a systemd timer unit.
     * \param unit  Name of the systemd timer without extension.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to start a systemd timer unit and returns a pointer to
     * the created object. The creating object will be the parent. \a unit has to be the unit
     * file name without extension like \c wp-cron. The extension \c .timer will automatically
     * be added.
     *
     * \sa startStopServiceOrTimer(), stopTimer()
     */
    QProcess* startTimer(const QString &unit);
    /*!
     * \brief Stops a systemd timer unit.
     * \param unit  Name of the systemd timer without extension.
     * \return Pointer to a QProcess object.
     *
     * Creates a QProcess object to stop a systemd timer unit and returns a pointer to
     * the created object. The creating object will be the parent. \a unit has to be the unit
     * file name without extension like \c wp-cron. The extension \c .timer will automatically
     * be added.
     *
     * \sa startStopServiceOrTimer(), startTimer()
     */
    QProcess* stopTimer(const QString &unit);

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
