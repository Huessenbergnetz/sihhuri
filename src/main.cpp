/*
 * SPDX-FileCopyrightText: (C) 2021 Matthias Fehring / www.huessenbergnetz.de
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QVariantMap>
#include <QFileInfo>
#include <QFile>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTranslator>
#include <QLocale>

#include <cstring>
extern "C"
{
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>
}

#include "backupmanager.h"

void journaldMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    int prio = LOG_INFO;
    switch (type) {
    case QtDebugMsg:
        prio = LOG_DEBUG;
        break;
    case QtInfoMsg:
        prio = LOG_INFO;
        break;
    case QtWarningMsg:
        prio = LOG_WARNING;
        break;
    case QtCriticalMsg:
        prio = LOG_CRIT;
        break;
    case QtFatalMsg:
        prio = LOG_ALERT;
        break;
    }

//    const QString ctx = QString::fromLatin1(context.category);
//    const QString id = ctx == QLatin1String("default") ? QCoreApplication::applicationName() : QCoreApplication::applicationName() + QLatin1Char('.') + ctx;
    const QString id = QCoreApplication::applicationName();

#ifdef QT_DEBUG
    sd_journal_send("PRIORITY=%i", prio, "SYSLOG_FACILITY=%hhu", 1, "SYSLOG_IDENTIFIER=%s", qUtf8Printable(id), "SYSLOG_PID=%lli", QCoreApplication::applicationPid(), "MESSAGE=%s", qFormatLogMessage(type, context, msg).toUtf8().constData(), "CODE_FILE=%s", context.file, "CODE_LINE=%i", context.line, "CODE_FUNC=%s", context.function, NULL);
#else
    sd_journal_send("PRIORITY=%i", prio, "SYSLOG_FACILITY=%hhu", 1, "SYSLOG_IDENTIFIER=%s", qUtf8Printable(id), "SYSLOG_PID=%lli", QCoreApplication::applicationPid(), "MESSAGE=%s", qFormatLogMessage(type, context, msg).toUtf8().constData(), NULL);
#endif

    if (type == QtFatalMsg) {
        abort();
    }
}

QVariantMap loadConfig(const QString &filepath)
{
    QVariantMap map;

    QFileInfo cfi(filepath);

    if (Q_UNLIKELY(!cfi.exists())) {
        //: Error message, %1 will be replaced by the file path
        //% "Can not find configuration file at %1"
        qCritical("%s", qUtf8Printable(qtTrId("SIHHURI_CRIT_CONFIG_NOT_FOUND").arg(cfi.absoluteFilePath())));
        return map;
    }

    if (Q_UNLIKELY(!cfi.isReadable())) {
        //: Error message, %1 will be replaced by the file path
        //% "Can not read configuration file at %1. Permission denied."
        qCritical("%s", qUtf8Printable(qtTrId("SIHHURI_CRIT_CONFIG_UNREADABLE").arg(cfi.absoluteFilePath())));
        return map;
    }

    qDebug("Reading settings from %s", qUtf8Printable(cfi.absoluteFilePath()));

    QFile fi(filepath);

    if (Q_UNLIKELY(!fi.open(QIODevice::ReadOnly|QIODevice::Text))) {
        //: Error message, %1 will be replaced by the file path, %2 by the error string
        //% "Can not open configuration file at %1: %2"
        qCritical("%s", qUtf8Printable(qtTrId("SIHHURI_CRIT_CONFIG_FAILED_OPEN").arg(cfi.absoluteFilePath(), fi.errorString())));
        return map;
    }

    QJsonParseError jsonError;
    const QJsonDocument json = QJsonDocument::fromJson(fi.readAll(), &jsonError);
    fi.close();
    if (jsonError.error != QJsonParseError::NoError) {
        //: Error message, %1 will be replaced by the file path, %2 by the JSON error string
        //% "Failed to parse JSON configuration file at %1: %2"
        qCritical("%s", qUtf8Printable(qtTrId("SIHHURI_CRIT_JSON_PARSE").arg(cfi.absoluteFilePath(), jsonError.errorString())));
        return map;
    }

    if (!json.isObject()) {
        //: Error message, %1 will be replaced by the file path
        //% "JSON configuration file at %1 does not contain an object as root"
        qCritical("%s", qUtf8Printable(qtTrId("SIHHURI_CRIT_JSON_NO_OBJECT").arg(cfi.absoluteFilePath())));
        return map;
    }

    map = json.object().toVariantMap();

    return map;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    a.setOrganizationName(QStringLiteral("Huessenbergnetz"));
    a.setOrganizationDomain(QStringLiteral("huessenbergnetz.de"));
    a.setApplicationName(QStringLiteral("sihhuri"));
    a.setApplicationVersion(QStringLiteral(SIHHURI_VERSION));

    qSetMessagePattern(QStringLiteral("%{message}"));
    qInstallMessageHandler(journaldMessageOutput);

    {
        qDebug("Loading translations from %s", SIHHURI_TRANSDIR);
        const QLocale locale;
        auto trans = new QTranslator(&a);
        if (Q_LIKELY(trans->load(locale, QCoreApplication::applicationName(), QStringLiteral("_"), QStringLiteral(SIHHURI_TRANSDIR), QStringLiteral(".qm")))) {
            if (Q_UNLIKELY(!QCoreApplication::installTranslator(trans))) {
                qWarning("Can not install translator for locale %s", qUtf8Printable(locale.name()));
            }
        } else {
            qWarning("Can not load translations for locale %s", qUtf8Printable(locale.name()));
        }
    }

    QCommandLineParser parser;

    QCommandLineOption configPath(QStringList({QStringLiteral("c"), QStringLiteral("config")}),
                                  //: Option description in the cli help, %1 will be replaced by the default path
                                  //% "Path to the configuration file to be used. Default: %1"
                                  qtTrId("SIHHURI_CLI_OPT_CONFIG_PATH").arg(QStringLiteral(SIHHURI_CONFIGFILE)),
                                  //: Option value name in the cli help for the configuration file path
                                  //% "file"
                                  qtTrId("SIHHURI_CLI_OPT_CONFIG_PATH_VAL"),
                                  QStringLiteral(SIHHURI_CONFIGFILE));
    parser.addOption(configPath);

    QCommandLineOption type(QStringList({QStringLiteral("t"), QStringLiteral("type")}),
                            //: Option description in the cli help
                            //% "Perform the backup only for the specified types. Multiple types are separated by commas. By default, backups for all types will be performed."
                            qtTrId("SIHHURI_CLI_OPT_TYPE"),
                            //: Option value name in the cli help for the type
                            //% "type"
                            qtTrId("SIHHURI_CLI_OPT_TYPE_VAL"));
    parser.addOption(type);

    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(a);

    const QVariantMap config = loadConfig(parser.value(configPath));
    if (config.isEmpty()) {
        return 6;
    }

    QStringList typesList;
    if (parser.isSet(type)) {
        const QString types = parser.value(type);
        if (!types.isEmpty()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
            typesList = types.split(QLatin1Char(','), Qt::SkipEmptyParts);
#else
            typesList = types.split(QLatin1Char(','), QString::SkipEmptyParts);
#endif
        }
    }

    auto bm = new BackupManager(config, typesList, &a);
    bm->start();

    return a.exec();
}
