/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstractbackup.h"
#include <QTimer>
#include <QProcess>
#include <QLocale>
#include <QDir>
#include <QFileInfo>
#include <chrono>

AbstractBackup::AbstractBackup(const QString &type, const QString &configFile, const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : QObject(parent),
      m_options(options),
      m_type(type),
      m_configFileName(configFile),
      m_target(target),
      m_tempDir(tempDir)
{

}

AbstractBackup::~AbstractBackup() = default;

void AbstractBackup::start()
{
    QTimer::singleShot(0, this, &AbstractBackup::startBackup);
}

void AbstractBackup::startBackup()
{
    setStartTime();

    setObjectName(option(QStringLiteral("name")).toString());

    //% "Starting backup"
    logInfo(qtTrId("SIHHURI_INFO_START_BACKUP_ITEM"));

    const QString cf = option(QStringLiteral("configFile")).toString();
    if (!cf.isEmpty()) {
        m_configFileName = cf;
    }

    const QStringList dirs = option(QStringLiteral("directories")).toStringList();
    for (const QString &dir : dirs) {
        QString d = dir;
        if (d.endsWith(QLatin1Char('/'))) {
            d.chop(1);
        }

        QFileInfo dirFi(d);
        if (!dirFi.exists() || !dirFi.isDir()) {
            //% "%1 does not exist or is not a directory."
            logError(qtTrId("SIHHURI_CRIT_DIR_NOT_EXISTS").arg(d));
            emitFinished();
            return;
        }

        m_dirQueue.enqueue(d);

        if (!m_configFileName.isEmpty() && !m_configFileName.startsWith(QLatin1Char('/'))) {
            QFileInfo configFileFi(d + QLatin1Char('/') + m_configFileName);
            if (configFileFi.exists() && configFileFi.isFile()) {
                m_configFilePath = configFileFi.absoluteFilePath();
                m_configFileRoot = d;
                m_user = configFileFi.owner();
            }
        }
    }

    if (m_configFileName.startsWith(QLatin1Char('/'))) {
        m_configFilePath = m_configFileName;
    }

    if (!m_configFileName.isEmpty()) {

        if (m_configFilePath.isEmpty()) {
            //% "Can not find configuration file %1."
            logError(qtTrId("SIHHURI_CRIT_FAILED_FIND_CONFIG_FILE").arg(m_configFileName));
            emitFinished();
            return;
        }
    }

    if (!loadConfiguration()) {
        emitFinished();
        return;
    }

    m_timer = option(QStringLiteral("timer")).toString();

    isTimerServiceActive();
}

void AbstractBackup::isTimerServiceActive()
{

    if (m_timer.isEmpty()) {
        enableMaintenance();
        return;
    }

    const QString timerService = m_timer + QLatin1String(".service");

    auto systemctl = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
    systemctl->setProgram(QStringLiteral("systemctl"));
    systemctl->setArguments({QStringLiteral("--quiet"), QStringLiteral("is-active"), timerService});
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, timerService](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            if (m_tryCountIsServiceActive < m_maxTryIsServiceActive) {
                m_tryCountIsServiceActive++;
                //% "%1 is still active, waiting %2 seconds for it to finish."
                logInfo(qtTrId("SIHHURI_INFO_SERVICE_STILL_ACTIVE_WAITING").arg(timerService, QString::number(m_waitSecondsIsServiceActive)));
                // QTimer::singleShot(m_waitSecondsIsServiceActive * 1000, this, &AbstractBackup::isTimerServiceActive);
                QTimer::singleShot(std::chrono::seconds(m_waitSecondsIsServiceActive), this, &AbstractBackup::isTimerServiceActive);
            } else {
                //% "Waited %1 seconds for %2 to finish without success."
                logWarning(qtTrId("SIHHURI_WARN_SERVICE_ACTIVE_TOO_LONG").arg(QString::number(m_waitSecondsIsServiceActive * m_maxTryIsServiceActive), timerService));
                enableMaintenance();
            }
        } else if (exitCode != 0 && exitStatus == QProcess::NormalExit) {
            stopTimer();
        } else {
            //% "Failed to check if %1 is active."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_CHECK_ACTIVE_SERVICE").arg(timerService));
            stopTimer();
        }
    });
    systemctl->start();
}

void AbstractBackup::stopTimer()
{
    auto systemctl = stopSystemdTimer(m_timer);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        enableMaintenance();
    });
    systemctl->start();
}

void AbstractBackup::enableMaintenance()
{
    doBackup();
}

void AbstractBackup::doBackup()
{
    disableMaintenance();
}

void AbstractBackup::backupDirectories()
{
    if (m_dirQueue.empty()) {
        emit backupDirectoriesFinished(QPrivateSignal());
        return;
    }

    setStepStartTime();

    const QString dir = m_dirQueue.dequeue();

    //% "Started syncing %1."
    logInfo(qtTrId("SIHHURI_INFO_START_RSYNC").arg(dir));

    m_currentStats = BackupStats();
    m_currentStats.type = BackupStats::Directory;
    m_currentStats.id = dir;

    const std::pair<qint64,qint64> dirSizeBefore = getDirSize(target() + dir);
    m_currentStats.filesBefore = dirSizeBefore.first;
    m_currentStats.sizeBefore = dirSizeBefore.second;

    QLocale locale;
    //% "Current size of the backed up data for %1: Files: %2, Size: %3"
    logInfo(qtTrId("SIHHURI_INFO_CURRENT_DIR_SIZE").arg(dir, locale.toString(m_currentStats.filesBefore), locale.formattedDataSize(m_currentStats.sizeBefore)));

    auto rsync = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
    rsync->setProgram(QStringLiteral("rsync"));
    rsync->setArguments({QStringLiteral("-aR"), QStringLiteral("--delete"), QStringLiteral("--delete-after"), dir, target()});
    connect(rsync, &QProcess::readyReadStandardError, this, [this, rsync](){
        logCritical(QStringLiteral("rsync: %1").arg(QString::fromUtf8(rsync->readAllStandardError())));
    });
    connect(rsync, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, dir](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            const std::pair<qint64,qint64> dirSizeAfter = getDirSize(dir);
            m_currentStats.filesAfter = dirSizeAfter.first;
            m_currentStats.sizeAfter = dirSizeAfter.second;
            m_currentStats.timeUsed = getStepTimeUsed();
            QLocale locale;
            //% "Finished syncing %1 in %2 milliseconds: Files: %3, Size: %4"
            logInfo(qtTrId("SIHHURI_INFO_FINISHED_RSYNC").arg(dir, locale.toString(m_currentStats.timeUsed), locale.toString(m_currentStats.filesAfter), locale.formattedDataSize(m_currentStats.sizeAfter)));
            addStatistic(m_currentStats);
        } else {
            //% "Failed to sync %1."
            logError(qtTrId("SIHHURI_CRIT_FAILED_RSYNC").arg(dir));
        }
        backupDirectories();
    });
    rsync->start();
}

void AbstractBackup::disableMaintenance()
{
    startTimer();
}

void AbstractBackup::startTimer()
{
    if (m_timer.isEmpty()) {
        emitFinished();
        return;
    }

    auto systemctl = startSystemdTimer(m_timer);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        emitFinished();
    });
    systemctl->start();
}

std::vector<BackupStats> AbstractBackup::statistics() const
{
    return m_stats;
}

void AbstractBackup::addStatistic(const BackupStats &statistic)
{
    m_stats.push_back(statistic);
}

void AbstractBackup::logDebug(const QString &msg) const
{
    qDebug("%s", qUtf8Printable(logMsg(msg)));
}

void AbstractBackup::logInfo(const QString &msg) const
{
    qInfo("%s", qUtf8Printable(logMsg(msg)));
}

void AbstractBackup::logWarning(const QString &msg)
{
    m_warnings << msg;
    qWarning("%s", qUtf8Printable(logMsg(msg)));
}

void AbstractBackup::logCritical(const QString &msg) const
{
    qCritical("%s", qUtf8Printable(logMsg(msg)));
}

void AbstractBackup::logError(const QString &msg)
{
    m_errors << msg;
    qCritical("%s", qUtf8Printable(logMsg(msg)));
}

QString AbstractBackup::logMsg(const QString &msg) const
{
    const QString log = id() + QLatin1String(": ") + msg;
    return log;
}

void AbstractBackup::setStartTime()
{
    m_timeStart = std::chrono::high_resolution_clock::now();
}

qint64 AbstractBackup::getTimeUsed() const
{
    const std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timeStart).count());
}

void AbstractBackup::setStepStartTime()
{
    m_stepTimeStart = std::chrono::high_resolution_clock::now();
}

qint64 AbstractBackup::getStepTimeUsed() const
{
    const std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stepTimeStart).count());
}

std::pair<qint64, qint64> AbstractBackup::getDirSize(const QString &path) const
{
    qint64 entries = 0;
    qint64 size = 0;

    QDir dir(path);

    const auto dirEntries = dir.entryInfoList(QDir::Files|QDir::System|QDir::Hidden);
    for (const QFileInfo &fi : dirEntries) {
        entries++;
        size += fi.size();
    }

    const auto childDirs = dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden|QDir::NoSymLinks);
    for (const QString &child : childDirs) {
        const auto ret = getDirSize(path + QLatin1Char('/') + child);
        entries += ret.first;
        size += ret.second;
    }

    return std::make_pair(entries, size);
}

QVariant AbstractBackup::option(const QString &key, const QVariant &defValue) const
{
    return m_options.value(key, defValue);
}

QString AbstractBackup::target() const
{
    return m_target;
}

QString AbstractBackup::tempDir() const
{
    return m_tempDir;
}

QString AbstractBackup::configFilePath() const
{
    return m_configFilePath;
}

QString AbstractBackup::configFileRoot() const
{
    return m_configFileRoot;
}

QString AbstractBackup::user() const
{
    return m_user;
}

QQueue<QString> AbstractBackup::directoryQueue() const
{
    return m_dirQueue;
}

void AbstractBackup::setDirectoryQueue(const QQueue<QString> &queue)
{
    m_dirQueue = queue;
}

void AbstractBackup::emitFinished()
{
    QLocale locale;
    const auto timeUsed = getTimeUsed();
    //% "Finished backup in %1 milliseconds. Errors: %2, Warnings: %3"
    const QString msg = logMsg(qtTrId("SIHHURI_INFO_FINISHED_BACKUP_ITEM").arg(locale.toString(timeUsed), QString::number(m_errors.size()), QString::number(m_warnings.size())));
    if (!m_errors.empty()) {
        qCritical("%s", qUtf8Printable(msg));
    } else if (!m_warnings.empty()) {
        qWarning("%s", qUtf8Printable(msg));
    } else {
        qInfo("%s", qUtf8Printable(msg));
    }
    emit finished(QPrivateSignal());
}

QProcess *AbstractBackup::startStopServiceOrTimer(const QString &unit, bool stop)
{
    if (!stop) {
        //% "Starting %1."
        logInfo(qtTrId("SIHHURI_INFO_START_SERVICE_OR_TIMER").arg(unit));
    } else {
        //% "Stopping %1."
        logInfo(qtTrId("SIHHURI_INFO_STOP_SERVICE_OR_TIMER").arg(unit));
    }

    auto p = new QProcess(this); // NOLINT(cppcoreguidelines-owning-memory)
    p->setProgram(QStringLiteral("systemctl"));

    QStringList args;
    if (stop) {
        args << QStringLiteral("stop");
    } else {
        args << QStringLiteral("start");
    }
    args << unit;
    p->setArguments(args);

    return p;
}


QProcess* AbstractBackup::startServiceOrTimer(const QString &unit)
{
    auto systemctl = startStopServiceOrTimer(unit, false);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, unit](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 && exitStatus != QProcess::NormalExit) {
            //% "Failed to start %1."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_START_TIMER_OR_SERVICE").arg(unit));
        }
    });
    return systemctl;
}

QProcess* AbstractBackup::stopServiceOrTimer(const QString &unit)
{
    auto systemctl = startStopServiceOrTimer(unit, true);
    connect(systemctl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, unit](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitCode != 0 && exitStatus != QProcess::NormalExit) {
            //% "Failed to stop %1."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_STOP_TIMER_OR_SERVICE").arg(unit));
        }
    });
    return systemctl;
}

QProcess* AbstractBackup::startSystemdService(const QString &unit)
{
    const QString _unit = unit + QLatin1String(".service");
    return startServiceOrTimer(_unit);
}

QProcess* AbstractBackup::stopSystemdService(const QString &unit)
{
    const QString _unit = unit + QLatin1String(".service");
    return stopServiceOrTimer(_unit);
}

QProcess* AbstractBackup::startSystemdTimer(const QString &unit)
{
    const QString _unit = unit + QLatin1String(".timer");
    return startServiceOrTimer(_unit);
}

QProcess* AbstractBackup::stopSystemdTimer(const QString &unit)
{
    const QString _unit = unit + QLatin1String(".timer");
    return stopServiceOrTimer(_unit);
}

QStringList AbstractBackup::errors() const
{
    return m_errors;
}

QStringList AbstractBackup::warnings() const
{
    return m_warnings;
}

QString AbstractBackup::id() const
{
    const QString id = objectName().isEmpty() ? m_type : QStringLiteral("%1(%2)").arg(m_type, objectName());
    return id;
}

#include "moc_abstractbackup.cpp"
