// Nickel's library database: reading the recently read sideloaded books and
// their positions, and storing a position through Nickel's own API.
#ifndef KOSYNC_NICKELDB_H
#define KOSYNC_NICKELDB_H

#include <QList>
#include <QString>

namespace kosync {

struct BookRow {
    QString contentId;   // "file:///mnt/onboard/..."
    QString title, author, mimeType;
    QString chapter;     // ChapterIDBookmarked, raw
    QString dateLastRead;
    int percent = 0;     // ___PercentRead
    int status = 0;      // ReadStatus
    bool valid = false;

    QString filePath() const { return contentId.startsWith("file:///") ? contentId.mid(7) : QString(); }
    bool kepub() const { return mimeType.contains("kobo", Qt::CaseInsensitive) || filePath().endsWith(".kepub.epub", Qt::CaseInsensitive); }
    // Only EPUB files are synced (comics and PDFs have no XPointers).
    bool epub() const { return mimeType.contains("epub", Qt::CaseInsensitive) || filePath().endsWith(".epub", Qt::CaseInsensitive); }
    qint64 modifiedMs() const;
};

// The Nickel row in the form Kobalt stores as its "nickel_progress" baseline.
struct NativeSnapshot {
    QString book, location;
    qint64 modifiedMs = 0;
    int status = 0;
    bool valid = false;
};

namespace nickeldb {

QString databasePath();
// Sideloaded books that have been opened, by last read time, newest first.
QList<BookRow> recentBooks(int limit);
// Every sideloaded book, opened ones first.
QList<BookRow> allBooks(int limit);
BookRow row(const QString &contentId);
NativeSnapshot snapshot(const BookRow &row);

// "OEBPS/ch1.xhtml#kobo.3.1" style location from ChapterIDBookmarked, with
// the book prefix some firmware versions add removed (Kobalt's nickel4::chapter).
QString chapterLocation(const QString &contentId, const QString &raw);
// Percent-encoded form of a location, as Kobalt records it (nickel4::shared).
QString sharedLocation(const QString &raw);
// Splits "path#fragment".
void splitLocation(const QString &location, QString *path, QString *fragment);

// Stores `location` ("path" or "path#fragment"), the percentage and the status
// for a sideloaded book through Nickel's Volume API, inside Nickel's
// transaction. Nothing may have the book open. Returns false with an error.
bool writePosition(const QString &contentId, const QString &location, double percent, bool finished, QString *error);

// True while Nickel's own reader is on screen.
bool readingViewActive();

}
}

#endif
