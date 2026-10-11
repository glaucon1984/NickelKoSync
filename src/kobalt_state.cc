#include "kobalt_state.h"

#include "nickeldb.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace kosync {
namespace kobalt {
namespace {

const char *const kobaltDirectory = "/mnt/onboard/.adds/kobalt";
const qint64 maxStateBytes = 16384;

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxStateBytes)
        return QJsonObject();
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

}

QString directory() {
    return QString::fromLatin1(kobaltDirectory);
}

bool installed() {
    return QFileInfo(directory()).isDir();
}

QString statePath(const QString &contentId) {
    const QByteArray hash = QCryptographicHash::hash(contentId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return directory() + '/' + QString::fromLatin1(hash) + ".json";
}

QString activeBook() {
    return readJson(directory() + "/active-session.json").value("book").toString();
}

State readState(const QString &contentId) {
    State state;
    const QString path = statePath(contentId);
    if (!QFile::exists(path))
        return state;
    const QJsonObject object = readJson(path);
    if (object.value("book").toString() != contentId)
        return state;
    state.exists = true;
    state.raw = object;
    state.position = object.value("position").toString();
    state.modifiedMs = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    const QJsonObject native = object.value("nickel_progress").toObject();
    state.nativeLocation = native.value("location").toString();
    state.nativeModifiedMs = native.value("modified").toString().toLongLong();
    return state;
}

bool writePosition(const QString &contentId, const QString &position, const NativeSnapshot &native, QString *error) {
    const QString path = statePath(contentId);
    QJsonObject object = readJson(path);
    if (object.value("book").toString() != contentId) {
        *error = "Kobalt has no saved state for this book";
        return false;
    }
    object["position"] = position;
    // Kobalt prefers Nickel's position when Nickel's row is newer than this
    // baseline; recording the current row keeps the stored position in charge.
    QJsonObject baseline;
    baseline["book"] = native.book;
    baseline["location"] = native.location;
    baseline["modified"] = QString::number(native.modifiedMs);
    baseline["status"] = native.status;
    baseline["valid"] = native.valid;
    object["nickel_progress"] = baseline;
    object["sync_pending"] = false;
    object["finished_position"] = QString();
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(object).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = "cannot write " + path;
        return false;
    }
    return true;
}

}
}
