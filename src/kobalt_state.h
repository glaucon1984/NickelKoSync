// Kobalt's saved state for a book. Since Kobalt v1.0-beta.4 it is one JSON
// record per book in /mnt/onboard/.adds/kobalt/kobalt.sqlite (table
// book_state, keyed by Nickel's content ID); earlier versions kept the same
// JSON in a file named by the SHA-256 of the content ID, which beta 4 imports
// at its first reading request. Formats follow Kobalt's
// storage/reader_storage.cpp and reading/reading_book.cpp. The active-session
// marker exists while Kobalt has a book open.
#ifndef KOSYNC_KOBALT_STATE_H
#define KOSYNC_KOBALT_STATE_H

#include <QJsonObject>
#include <QString>

namespace kosync {

struct NativeSnapshot;

namespace kobalt {

QString directory();
// For tests: use another folder in place of /mnt/onboard/.adds/kobalt.
void setDirectory(const QString &path);
QString statePath(const QString &contentId);
QString databasePath();
bool hasDatabase();
// The content ID of the book Kobalt has open, or empty.
QString activeBook();
bool installed();

struct State {
    bool exists = false;
    QString position;          // CREngine XPointer
    // The state file's modification time; 0 for a database record, which
    // carries no time of its own (nickel_progress tells which row Kobalt
    // last wrote).
    qint64 modifiedMs = 0;
    int percent = -1;          // Kobalt's own percentage of `position` (beta 4), -1 when absent
    QString nativeLocation;    // nickel_progress.location
    qint64 nativeModifiedMs = 0;
    QJsonObject raw;
};
State readState(const QString &contentId);

// Stores a new position and the Nickel row it corresponds to, so Kobalt opens
// at `position` instead of preferring Nickel's location. The record (database
// row or legacy file) must exist.
bool writePosition(const QString &contentId, const QString &position, const NativeSnapshot &native, QString *error);

}
}

#endif
