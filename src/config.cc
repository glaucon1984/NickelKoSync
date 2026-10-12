#include "config.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStringList>

namespace kosync {
namespace {

QString lower(const QVariant &value) {
    return value.toString().trimmed().toLower();
}

}

bool readConfig(const QString &path, Config *config, QString *problem) {
    *config = Config();
    if (!QFile::exists(path)) {
        if (problem)
            *problem = "config.ini is missing";
        return false;
    }
    QSettings ini(path, QSettings::IniFormat);
    ini.setIniCodec("UTF-8");
    if (ini.status() != QSettings::NoError) {
        if (problem)
            *problem = "config.ini could not be parsed";
        return false;
    }

    ini.beginGroup("server");
    config->url = ini.value("url", "https://sync.koreader.rocks").toString().trimmed();
    while (config->url.endsWith('/'))
        config->url.chop(1);
    if (!config->url.isEmpty() && !config->url.contains("://"))
        config->url.prepend("https://");
    config->username = ini.value("username").toString().trimmed();
    const QString password = ini.value("password").toString();
    const QString passwordMd5 = lower(ini.value("password_md5"));
    if (!passwordMd5.isEmpty())
        config->userKey = passwordMd5;
    else if (!password.isEmpty())
        config->userKey = QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5).toHex());
    config->deviceName = ini.value("device", "Kobo").toString().trimmed();
    config->insecure = ini.value("insecure", false).toBool();
    config->caFile = ini.value("ca_file").toString().trimmed();
    ini.endGroup();

    ini.beginGroup("sync");
    config->documentId = lower(ini.value("document_id", "binary")) == "filename" ? Config::DocumentFilename : Config::DocumentBinary;
    const QString pull = lower(ini.value("pull", "newer"));
    config->pull = pull == "forward" ? Config::PullForward : pull == "off" || pull == "false" ? Config::PullOff : Config::PullNewer;
    config->push = ini.value("push", true).toBool();
    config->recentBooks = qBound(1, ini.value("recent_books", 3).toInt(), 20);
    config->pullIntervalMinutes = qBound(1, ini.value("pull_interval_minutes", 15).toInt(), 24 * 60);
    config->sendMetadata = ini.value("send_metadata", false).toBool();
    config->onOpen = ini.value("on_open", true).toBool();
    ini.endGroup();

    ini.beginGroup("wifi");
    config->connectWifi = lower(ini.value("mode", "connect")) != "keep";
    config->connectCooldownMinutes = qBound(1, ini.value("cooldown_minutes", 10).toInt(), 24 * 60);
    ini.endGroup();

    ini.beginGroup("options");
    config->verbose = ini.value("verbose", false).toBool();
    config->notify = ini.value("notify", true).toBool();
    config->menu = ini.value("menu", true).toBool();
    config->menuLog = ini.value("menu_log", false).toBool();
    config->maxBooks = qBound(1, ini.value("max_books", 300).toInt(), 2000);
    ini.endGroup();

    if (problem) {
        if (config->username.isEmpty())
            *problem = "username is not set";
        else if (config->userKey.isEmpty())
            *problem = "password (or password_md5) is not set";
        else
            problem->clear();
    }
    return true;
}

}

namespace kosync {

bool setConfigValue(const QString &path, const QString &section, const QString &key, const QString &value) {
    QFile file(path);
    QString content;
    if (file.open(QIODevice::ReadOnly))
        content = QString::fromUtf8(file.readAll());
    file.close();
    const QString newline = content.contains("\r\n") ? "\r\n" : "\n";
    QStringList lines = content.split(QRegularExpression("\r?\n"));
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    const QRegularExpression header("^\\s*\\[([^\\]]+)\\]");
    const QRegularExpression assignment("^\\s*" + QRegularExpression::escape(key) + "\\s*=");
    int sectionStart = -1, sectionEnd = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const QRegularExpressionMatch match = header.match(lines[i]);
        if (!match.hasMatch())
            continue;
        if (sectionStart >= 0) {
            sectionEnd = i;
            break;
        }
        if (match.captured(1).trimmed().compare(section, Qt::CaseInsensitive) == 0)
            sectionStart = i;
    }
    const QString line = key + " = " + value;
    if (sectionStart < 0) {
        if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
            lines.append(QString());
        lines.append('[' + section + ']');
        lines.append(line);
    } else {
        int found = -1;
        for (int i = sectionStart + 1; i < sectionEnd; ++i)
            if (assignment.match(lines[i]).hasMatch()) {
                found = i;
                break;
            }
        if (found >= 0) {
            lines[found] = line;
        } else {
            int insert = sectionEnd;
            while (insert > sectionStart + 1 && lines[insert - 1].trimmed().isEmpty())
                --insert;
            lines.insert(insert, line);
        }
    }
    QSaveFile output(path);
    const QByteArray bytes = (lines.join(newline) + newline).toUtf8();
    return output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size() && output.commit();
}

}
