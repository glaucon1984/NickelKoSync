#include "log.h"

#include <NickelHook.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifndef KS_CONFIG_DIR
#define KS_CONFIG_DIR "/mnt/onboard/.adds/kosync"
#endif

namespace kosync {
namespace {

bool verbose = false;
const char *const logFile = KS_CONFIG_DIR "/log.txt";
const qint64 rotateBytes = 256 * 1024;

void append(const QString &message) {
    QDir().mkpath(KS_CONFIG_DIR);
    if (QFileInfo(logFile).size() > rotateBytes) {
        QFile::remove(QString(logFile) + ".1");
        QFile::rename(logFile, QString(logFile) + ".1");
    }
    QFile file(logFile);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "  " + message + "\n";
    file.write(line.toUtf8());
}

}

void setVerboseLogging(bool on) {
    verbose = on;
}

bool verboseLogging() {
    return verbose;
}

void log(const QString &message) {
    nh_log("kosync: %s", message.toUtf8().constData());
    append(message);
}

void debug(const QString &message) {
    if (verbose)
        log(message);
}

}
