/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dbbackup.h"
#include <QTextStream>
#include <QDir>
#include <QCryptographicHash>

DbBackup::DbBackup(const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : AbstractBackup(QStringLiteral("MariaDB"), QString(), target, tempDir, options, parent)
{

}

DbBackup::DbBackup(const QString &type, const QString &configFile, const QString &target, const QString &tempDir, const QVariantMap &options, QObject *parent)
    : AbstractBackup(type, configFile, target, tempDir, options, parent)
{

}

DbBackup::~DbBackup() = default;

bool DbBackup::loadConfiguration()
{
    const QString type = option(QStringLiteral("type")).toString();
    if (type.compare(QLatin1String("mysql")) == 0) {
        setDbType(MySQL);
    } else if (type.compare(QLatin1String("mariadb")) == 0) {
        setDbType(MariaDB);
    } else if (type.compare(QLatin1String("postgresql")) == 0 || type.compare(QLatin1String("pgsql")) == 0) {
        setDbType(PostgreSQL);
    } else {
        //% "%1 is not a supported database type."
        logError(qtTrId("SIHHURI_CRIT_INVALID_DB_TYPE_NAME").arg(type));
        return false;
    }
    setDbName(option(QStringLiteral("name")).toString());
    setDbUser(option(QStringLiteral("user")).toString());
    setDbPassword(option(QStringLiteral("password")).toString());
    setDbHost(option(QStringLiteral("host"), QStringLiteral("localhost")).toString());
    setDbPort(option(QStringLiteral("port"), 3306).toInt());

    return true;
}

void DbBackup::doBackup()
{
    connect(this, &DbBackup::backupDatabaseFinished, this, &DbBackup::onBackupDatabaseFinished);
    connect(this, &DbBackup::backupDatabaseFailed, this, &DbBackup::onBackupDatabaseFinished);
    backupDatabase();
}

void DbBackup::onBackupDatabaseFinished()
{
    disableMaintenance();
}

void DbBackup::backupDatabase()
{
    switch (m_type) {
    case MySQL:
        backupMySql();
        break;
    case MariaDB:
        backupMariaDb();
        break;
    case PostgreSQL:
        backupPgSql();
        break;
    case SQLite:
        emit backupDatabaseFinished(QPrivateSignal());
        break;
    default:
        //% "Invalid database type or no database set."
        logError(qtTrId("SIHHURI_CRIT_INVALID_DB_TYPE"));
        emit backupDatabaseFailed(QPrivateSignal());
        break;
    }
}

QString DbBackup::dbDirPath() const
{
    return target() + QLatin1String("/Databases");
}

void DbBackup::backupMySql()
{
    //% "Starting dump of MySQL/MariaDB database %1."
    logInfo(qtTrId("SIHHURI_INFO_START_DUMP_MYSQL").arg(dbName()));
    setStepStartTime();

    if (!m_dbConfigFile.open()) {
        //% "Failed to open temporary file for database configuration: %1"
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_TEMP_DBCONFFILE").arg(m_dbConfigFile.errorString()));
        emit backupDatabaseFailed(QPrivateSignal());
        return;
    }

    {
        QTextStream confOut(&m_dbConfigFile);
        confOut << "[client]" << endl;
        confOut << "user=\"" << dbUser() << "\"" << endl;
        confOut << "password=\"" << dbPassword() << "\"" << endl;
        if (dbHost().startsWith(QLatin1Char('/'))) {
            confOut << "socket=\"" << dbHost() << "\"" << endl;
        } else {
            if (dbHost() != QLatin1String("localhost")) {
                confOut << "host=\"" << dbHost() << "\"" << endl;
            }
            if (dbPort() != 3306) {
                confOut << "port=\"" << dbPort() << "\"" << endl;
            }
        }
        confOut.flush();
    }

    m_dbConfigFile.close();
    m_dbConfigFile.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);

    QDir dbDir(dbDirPath());
    if (!dbDir.mkpath(dbDir.path())) {
        //% "Failed to create database directory."
        logError(qtTrId("SIHHURI_CRIT_FAILED_CREATE_DBDIR"));
        emit backupDatabaseFailed(QPrivateSignal());
        return;
    }

    m_dumpFile = new QFile(this);
    m_dumpFile->setFileName(dbDir.absoluteFilePath(QLatin1String("mysql_") + dbName() + QLatin1String(".sql")));

    if (!m_dumpFile->open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)) {
        //% "Failed to open %1 to store MySQL/MariaDB database dump: %2"
        logError(qtTrId("SIHHURI_CRIT_FAILED_OPEN_DUMPFILE_WRITE").arg(m_dumpFile->fileName(), m_dumpFile->errorString()));
        emit backupDatabaseFailed(QPrivateSignal());
        return;
    }

    const QString defFileArg = QLatin1String("--defaults-file=") + m_dbConfigFile.fileName();
    QStringList dumpArgs({defFileArg, dbName()});

    auto mysqldump = new QProcess(this);
    mysqldump->setProgram(QStringLiteral("mysqldump"));
    mysqldump->setArguments(dumpArgs);
    connect(mysqldump, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &DbBackup::onDatabaseDumpFinished);
    connect(mysqldump, &QProcess::readyReadStandardError, this, [=](){
        logCritical(QStringLiteral("mysqldump: %1").arg(QString::fromUtf8(mysqldump->readAllStandardError())));
    });
    connect(mysqldump, &QProcess::readyReadStandardOutput, this, [=](){
        m_dumpFile->write(mysqldump->readAllStandardOutput());
    });
    mysqldump->start();
}

void DbBackup::backupMariaDb()
{
    backupMySql();
}

void DbBackup::backupPgSql()
{

}

void DbBackup::onDatabaseDumpFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_dumpFile->close();
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        const qint64 timeUsed = getStepTimeUsed();
        QLocale locale;
        QFileInfo fi(m_dumpFile->fileName());
        //% "Finished dump of MySQL/MariaDB database %1 with %2 in %3 milliseconds."
        logInfo(qtTrId("SIHHURI_INFO_FINISHED_DUMP_MYSQL").arg(dbName(), locale.formattedDataSize(fi.size()), locale.toString(timeUsed)));
        hashDatabase();
    } else {
        //% "Failed to create MySQL/MariaDB database dump of %1."
        logError(qtTrId("SIHHURI_CRIT_FAILED_DBDUMP").arg(dbName()));
        emit backupDatabaseFailed(QPrivateSignal());
    }
}

void DbBackup::hashDatabase()
{
    setStepStartTime();
    const QFileInfo dumpFileFi(m_dumpFile->fileName());
    if (m_dumpFile->open(QIODevice::ReadOnly)) {

        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(m_dumpFile);
        const QByteArray hash = hasher.result();
        const QString sha256sum = QString::fromLatin1(hash.toHex());
        const qint64 timeUsed = getStepTimeUsed();

        QLocale locale;
        //% "Calculated SHA256 hash sum of %1 in %2 milliseconds: %3"
        logInfo(qtTrId("SIHHURI_INFO_FINISHED_SHASUM_MYSQLDUMP").arg(dumpFileFi.fileName(), locale.toString(timeUsed), sha256sum));
        m_dumpFile->close();

        QFile hashValuesFile(dbDirPath() + QLatin1String("/sha256sums.txt"));
        if (hashValuesFile.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Append)) {

            QTextStream out(&hashValuesFile);
            out << sha256sum << " " << dumpFileFi.fileName() << endl;
            out.flush();

            hashValuesFile.close();
        } else {
            //% "Failed to open %1 for writing SHA256 hash values."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_OPEN_SHA256SUMS_FILE").arg(hashValuesFile.fileName()));
        }
    } else {
        //% "Failed to open MySQL/MariaDB database dump file %1, omitting SHA256 hash sum calculation."
        logWarning(qtTrId("SIHHURI_WARN_FAILD_OPEN_MYSQL_DUMP_OMIT_HASH").arg(m_dumpFile->fileName()));
    }
    compressDatabase();
}

void DbBackup::compressDatabase()
{
    const QFileInfo dumpFileFi(m_dumpFile->fileName());

    //% "Starting compression of MySQL/MariaDB database dump %1."
    logInfo(qtTrId("SIHHURI_INFO_START_COMPRESS_MYSQL").arg(dumpFileFi.fileName()));
    setStepStartTime();

    auto pixz = new QProcess(this);
    pixz->setWorkingDirectory(dbDirPath());
    pixz->setProgram(QStringLiteral("pixz"));

    const QString xzFileName  = dumpFileFi.fileName() + QLatin1String(".pxz");
    pixz->setArguments({dumpFileFi.fileName(), xzFileName});
    connect(pixz, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &DbBackup::onCompressDatabaseFinished);
    connect(pixz, &QProcess::readyReadStandardError, this, [=](){
        logCritical(QStringLiteral("pixz: %1").arg(QString::fromUtf8(pixz->readAllStandardError())));
    });
    pixz->start();
}

void DbBackup::onCompressDatabaseFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        QFileInfo xzFi(m_dumpFile->fileName() + QLatin1String(".pxz"));
        if (!m_dumpFile->remove()) {
            //% "Failed to remove uncompressed MySQL/MariaDB database dump file %1."
            logWarning(qtTrId("SIHHURI_WARN_FAILED_REMOVE_UNCOMPRESSED_MYSQL_DUMP_FILE").arg(m_dumpFile->fileName()));
        }
        delete m_dumpFile;
        m_dumpFile = nullptr;
        const qint64 timeUsed = getStepTimeUsed();
        QLocale locale;
        //% "Finished compression of MySQL/MariaDB database dump %1 with %2 in %3 milliseconds."
        logInfo(qtTrId("SIHHURI_INFO_FINISHED_COMPRESS_MYSQL").arg(dbName(), locale.formattedDataSize(xzFi.size()), locale.toString(timeUsed)));
    } else {
        //% "Failed to compress MySQL/MariaDB database dump %1."
        logWarning(qtTrId("SIHHURI_CRIT_FAILED_FAILED_COMPRESS_MYSQL").arg(dbName()));
    }
    emit backupDatabaseFinished(QPrivateSignal());
}

void DbBackup::setDbType(DbBackup::Type type)
{
    m_type = type;
}

DbBackup::Type DbBackup::dbType() const
{
    return m_type;
}

void DbBackup::setDbName(const QString &dbName)
{
    m_dbName = dbName;
}

QString DbBackup::dbName() const
{
    return m_dbName;
}

void DbBackup::setDbUser(const QString &dbUser)
{
    m_dbUser = dbUser;
}

QString DbBackup::dbUser() const
{
    return m_dbUser;
}

void DbBackup::setDbPassword(const QString &dbPassword)
{
    m_dbPass = dbPassword;
}

QString DbBackup::dbPassword() const
{
    return m_dbPass;
}

void DbBackup::setDbHost(const QString &dbHost)
{
    m_dbHost = dbHost;
}

QString DbBackup::dbHost() const
{
    return m_dbHost;
}

void DbBackup::setDbPort(int dbPort)
{
    m_dbPort = dbPort;
}

int DbBackup::dbPort() const
{
    return m_dbPort;
}

#include "moc_dbbackup.cpp"
