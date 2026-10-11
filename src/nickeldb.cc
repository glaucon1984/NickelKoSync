#include "nickeldb.h"

#include "log.h"
#include "nickel.h"

#include <QDateTime>
#include <QFile>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUrl>
#include <QVariant>
#include <QWidget>

namespace kosync {

qint64 BookRow::modifiedMs() const {
    const QDateTime date = QDateTime::fromString(dateLastRead, Qt::ISODate);
    return date.isValid() ? date.toMSecsSinceEpoch() : 0;
}

namespace nickeldb {
namespace {

const char *const defaultDatabase = "/mnt/onboard/.kobo/KoboReader.sqlite";
quint64 connectionCounter = 0;

QString localBookPath(const QString &book) {
    return book.startsWith("file:///") ? book.mid(7) : QString();
}

// Kobalt's nickel4::shared(): percent-encoded path and fragment.
QString sharedLocation(const QString &raw) {
    const int hash = raw.indexOf('#');
    const QString path = QString::fromLatin1(QUrl::toPercentEncoding(raw.left(hash < 0 ? raw.size() : hash), "/"));
    return hash < 0 ? path : path + '#' + QString::fromLatin1(QUrl::toPercentEncoding(raw.mid(hash + 1)));
}

BookRow rowFromQuery(QSqlQuery &query) {
    BookRow row;
    row.contentId = query.value(0).toString();
    row.title = query.value(1).toString();
    row.author = query.value(2).toString();
    row.chapter = query.value(3).toString();
    row.percent = query.value(4).toInt();
    row.dateLastRead = query.value(5).toString();
    row.status = query.value(6).toInt();
    row.mimeType = query.value(7).toString();
    row.valid = !row.contentId.isEmpty();
    return row;
}

const char *const columns =
    "ContentID, Title, Attribution, ChapterIDBookmarked, ___PercentRead, DateLastRead, ReadStatus, MimeType";

template <class F> void withReadOnlyDatabase(F body) {
    const QString name = QString("kosync-%1").arg(++connectionCounter);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(databasePath());
        db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=500");
        if (db.open())
            body(db);
        else
            log("cannot open Nickel's database: " + db.lastError().text());
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

struct VolumeGuard {
    nickel::Volume volume;
    bool live = false;
    ~VolumeGuard() {
        if (live && nickel::api().destroyVolume)
            nickel::api().destroyVolume(&volume);
    }
};

}

QString databasePath() {
    const nickel::Api &api = nickel::api();
    if (api.connection && api.device && api.dbName) {
        const QSqlDatabase existing = api.connection(api.dbName(api.device()), false);
        if (existing.isValid() && !existing.databaseName().isEmpty())
            return existing.databaseName();
    }
    return QString::fromLatin1(defaultDatabase);
}

QList<BookRow> recentBooks(int limit) {
    QList<BookRow> rows;
    withReadOnlyDatabase([&](QSqlDatabase &db) {
        QSqlQuery query(db);
        query.prepare(QString("SELECT %1 FROM content WHERE ContentType = 6 AND ContentID LIKE 'file:///%' "
                              "AND DateLastRead IS NOT NULL AND DateLastRead != '' "
                              "ORDER BY DateLastRead DESC LIMIT ?")
                          .arg(columns));
        query.addBindValue(limit);
        if (!query.exec()) {
            log("recent books query failed: " + query.lastError().text());
            return;
        }
        while (query.next())
            rows.append(rowFromQuery(query));
    });
    return rows;
}

BookRow row(const QString &contentId) {
    BookRow result;
    withReadOnlyDatabase([&](QSqlDatabase &db) {
        QSqlQuery query(db);
        query.prepare(QString("SELECT %1 FROM content WHERE ContentID = ? AND ContentType = 6").arg(columns));
        query.addBindValue(contentId);
        if (query.exec() && query.next())
            result = rowFromQuery(query);
    });
    return result;
}

QString chapterLocation(const QString &contentId, const QString &raw) {
    const QString prefixes[2] = {contentId + '!', localBookPath(contentId) + '!'};
    for (int i = 0; i < 2; ++i) {
        const QString &prefix = prefixes[i];
        if (prefix.size() < 2 || !raw.startsWith(prefix))
            continue;
        const int hash = raw.indexOf('#', prefix.size());
        QString path = raw.mid(prefix.size(), hash < 0 ? -1 : hash - prefix.size());
        path.replace('!', '/');
        return path + (hash < 0 ? QString() : raw.mid(hash));
    }
    return raw;
}

void splitLocation(const QString &location, QString *path, QString *fragment) {
    const int hash = location.indexOf('#');
    *path = hash < 0 ? location : location.left(hash);
    *fragment = hash < 0 ? QString() : location.mid(hash + 1);
}

NativeSnapshot snapshot(const BookRow &row) {
    NativeSnapshot value;
    value.book = row.contentId;
    value.status = row.status;
    value.modifiedMs = row.modifiedMs();
    value.location = sharedLocation(chapterLocation(row.contentId, row.chapter));
    value.valid = !row.contentId.isEmpty();
    return value;
}

bool writePosition(const QString &contentId, const QString &location, double percent, bool finished, QString *error) {
    const nickel::Api &api = nickel::api();
    if (!api.volumeApi()) {
        *error = "Nickel's volume API is not available on this firmware";
        return false;
    }
    if (QMetaType::sizeOf(QMetaType::type("Volume")) != int(sizeof(nickel::Volume))) {
        *error = "Nickel's Volume type has an unexpected size";
        return false;
    }
    if (location.isEmpty() || location.contains("##")) {
        *error = "no position to store";
        return false;
    }
    const QString db = api.dbName(api.device());
    if (!api.begin(db)) {
        *error = "Nickel's database is busy";
        return false;
    }
    bool committed = false;
    {
        VolumeGuard guard;
        api.getVolume(&guard.volume, contentId, db);
        guard.live = true;
        const QVariantMap original = api.values(&guard.volume);
        if (original.value("ContentID").toString() != contentId) {
            api.end(db, false);
            *error = "Nickel does not know this book";
            return false;
        }
        const qint64 previous = QDateTime::fromString(original.value("DateLastRead").toString(), Qt::ISODate).toMSecsSinceEpoch();
        const QDateTime stamp =
            QDateTime::fromMSecsSinceEpoch(qMax(QDateTime::currentMSecsSinceEpoch(), previous + 1000), Qt::UTC);
        api.bookmark(&guard.volume, location, original.value("adobe_location").toInt(), stamp);
        api.status(&guard.volume, finished ? 2 : 1);
        api.firstTime(&guard.volume, QVariant(false));
        api.percent(&guard.volume, QVariant(finished ? 100.0 : qBound(0.0, percent, 99.0)));
        const QVariantMap after = api.values(&guard.volume);
        QVariantMap changed;
        for (QVariantMap::const_iterator it = after.constBegin(); it != after.constEnd(); ++it)
            if (it.value() != original.value(it.key()))
                changed.insert(it.key(), it.value());
        const bool saved = !changed.isEmpty() && api.update(contentId, changed, QVariantMap{{"ContentID", contentId}}, db);
        committed = saved && api.end(db, true);
        if (!committed) {
            api.end(db, false);
            api.refresh(db);
        }
    }
    if (!committed) {
        *error = "Nickel did not store the position";
        return false;
    }
    return true;
}

bool readingViewActive() {
    const nickel::Api &api = nickel::api();
    if (!api.mainWindowController || !api.currentView)
        return false;
    void *controller = api.mainWindowController();
    if (!controller)
        return false;
    QWidget *view = api.currentView(controller);
    return view && view->objectName() == "ReadingView";
}

}
}
