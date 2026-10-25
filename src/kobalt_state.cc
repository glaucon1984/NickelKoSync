#include "kobalt_state.h"

#include "log.h"
#include "nickeldb.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace kosync {
namespace kobalt {
namespace {

const char *const defaultDirectory = "/mnt/onboard/.adds/kobalt";
// Kobalt's limit for a book_state payload (and what the mod reads from a file).
const qint64 maxStateBytes = 16384;
QString directoryOverride;
int connectionCounter = 0;

QJsonObject parseState(const QByteArray &bytes) {
    if (bytes.size() > maxStateBytes)
        return QJsonObject();
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxStateBytes)
        return QJsonObject();
    return parseState(file.readAll());
}

// Runs `body` on its own connection to Kobalt's database. Kobalt opens and
// closes a connection per operation too (DELETE journal, 1.5 s busy timeout),
// so neither holds the file across the other's transactions. `opened` tells a
// connection failure from a false `body`.
template <class F> bool withDatabase(bool readOnly, bool *opened, F body) {
    const QString name = QString("kosync-kobalt-%1").arg(++connectionCounter);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(databasePath());
        db.setConnectOptions(readOnly ? "QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1500" : "QSQLITE_BUSY_TIMEOUT=1500");
        *opened = db.open();
        if (*opened)
            ok = body(db);
        else
            debug("cannot open Kobalt's database: " + db.lastError().text());
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

// The book's record as Kobalt reads it: format 1 and a JSON object whose
// "book" is the identity. `found` tells a missing row from a failed query.
bool selectRecord(QSqlDatabase &db, const QString &contentId, QJsonObject *object, bool *found, QString *error) {
    *found = false;
    QSqlQuery query(db);
    query.setForwardOnly(true);
    if (!query.prepare("SELECT format, CAST(payload AS BLOB) FROM book_state WHERE identity = ?")) {
        *error = "cannot read Kobalt's database: " + query.lastError().text();
        return false;
    }
    query.addBindValue(contentId);
    if (!query.exec()) {
        *error = "cannot read Kobalt's database: " + query.lastError().text();
        return false;
    }
    if (!query.next())
        return true;
    if (query.value(0).toInt() != 1) {
        *error = "Kobalt's record has an unsupported format";
        return false;
    }
    *object = parseState(query.value(1).toByteArray());
    if (object->value("book").toString() != contentId) {
        *error = "Kobalt's record does not belong to the book";
        return false;
    }
    *found = true;
    return true;
}

void fill(State *state, const QJsonObject &object) {
    state->exists = true;
    state->raw = object;
    state->position = object.value("position").toString();
    state->percent = object.contains("percent") ? object.value("percent").toInt(-1) : -1;
    const QJsonObject native = object.value("nickel_progress").toObject();
    state->nativeLocation = native.value("location").toString();
    state->nativeModifiedMs = native.value("modified").toString().toLongLong();
}

void setPosition(QJsonObject *object, const QString &position, const QJsonObject &baseline) {
    (*object)["position"] = position;
    (*object)["nickel_progress"] = baseline;
    (*object)["sync_pending"] = false;
    (*object)["finished_position"] = QString();
}

}

QString directory() {
    return directoryOverride.isEmpty() ? QString::fromLatin1(defaultDirectory) : directoryOverride;
}

void setDirectory(const QString &path) {
    directoryOverride = path;
}

bool installed() {
    return QFileInfo(directory()).isDir();
}

QString statePath(const QString &contentId) {
    const QByteArray hash = QCryptographicHash::hash(contentId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return directory() + '/' + QString::fromLatin1(hash) + ".json";
}

QString databasePath() {
    return directory() + "/kobalt.sqlite";
}

bool hasDatabase() {
    return QFile::exists(databasePath());
}

QString activeBook() {
    return readJson(directory() + "/active-session.json").value("book").toString();
}

State readState(const QString &contentId) {
    State state;
    if (hasDatabase()) {
        QJsonObject object;
        bool found = false, opened = false;
        QString error;
        const bool ok = withDatabase(true, &opened, [&](QSqlDatabase &db) {
            return selectRecord(db, contentId, &object, &found, &error);
        });
        if (ok && found) {
            fill(&state, object);
            return state;
        }
        if (opened && !ok)
            debug(error);
    }
    // Before beta 4, or a book beta 4 has not imported yet.
    const QString path = statePath(contentId);
    if (!QFile::exists(path))
        return state;
    const QJsonObject object = readJson(path);
    if (object.value("book").toString() != contentId)
        return state;
    fill(&state, object);
    state.modifiedMs = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    return state;
}

bool writePosition(const QString &contentId, const QString &position, const NativeSnapshot &native, QString *error) {
    // Kobalt prefers Nickel's position when Nickel's row is newer than this
    // baseline; recording the current row keeps the stored position in charge.
    QJsonObject baseline;
    baseline["book"] = native.book;
    baseline["location"] = native.location;
    baseline["modified"] = QString::number(native.modifiedMs);
    baseline["status"] = native.status;
    baseline["valid"] = native.valid;
    error->clear();

    if (hasDatabase()) {
        bool found = false, opened = false;
        const bool ok = withDatabase(false, &opened, [&](QSqlDatabase &db) {
            QSqlQuery control(db);
            if (!control.exec("BEGIN IMMEDIATE")) {
                *error = "cannot lock Kobalt's database: " + control.lastError().text();
                return false;
            }
            QJsonObject object;
            bool committed = false;
            if (selectRecord(db, contentId, &object, &found, error) && found) {
                setPosition(&object, position, baseline);
                const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
                QSqlQuery update(db);
                // Kobalt's own statement: the payload as text, one more revision.
                if (bytes.size() > maxStateBytes) {
                    *error = "Kobalt's record would exceed its size limit";
                } else if (!update.prepare("UPDATE book_state SET payload = CAST(? AS TEXT), revision = revision + 1 "
                                           "WHERE identity = ?")) {
                    *error = "cannot write Kobalt's database: " + update.lastError().text();
                } else {
                    update.addBindValue(bytes);
                    update.addBindValue(contentId);
                    if (!update.exec() || update.numRowsAffected() != 1)
                        *error = "cannot write Kobalt's database: " + update.lastError().text();
                    else if (!control.exec("COMMIT"))
                        *error = "cannot commit to Kobalt's database: " + control.lastError().text();
                    else
                        committed = true;
                }
            }
            if (!committed)
                control.exec("ROLLBACK");
            return committed;
        });
        if (ok)
            return true;
        if (!opened) {
            *error = "cannot open Kobalt's database";
            return false;
        }
        if (found || !error->isEmpty())
            return false;
        // No row yet: the book may still be waiting in a legacy file.
    }

    const QString path = statePath(contentId);
    QJsonObject object = readJson(path);
    if (object.value("book").toString() != contentId) {
        *error = "Kobalt has no saved state for this book";
        return false;
    }
    setPosition(&object, position, baseline);
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
