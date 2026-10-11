// Kobalt's saved state: one JSON file per book in /mnt/onboard/.adds/kobalt,
// named by the SHA-256 of Nickel's content ID, plus the active-session marker
// that exists while Kobalt has a book open. Formats follow Kobalt's
// storage/reader_storage.cpp and reading/reading_book.cpp.
#ifndef KOSYNC_KOBALT_STATE_H
#define KOSYNC_KOBALT_STATE_H

#include <QJsonObject>
#include <QString>

namespace kosync {

struct NativeSnapshot;

namespace kobalt {

QString directory();
QString statePath(const QString &contentId);
// The content ID of the book Kobalt has open, or empty.
QString activeBook();
bool installed();

struct State {
    bool exists = false;
    QString position;          // CREngine XPointer
    qint64 modifiedMs = 0;     // the file's modification time
    QString nativeLocation;    // nickel_progress.location
    qint64 nativeModifiedMs = 0;
    QJsonObject raw;
};
State readState(const QString &contentId);

// Stores a new position and the Nickel row it corresponds to, so Kobalt opens
// at `position` instead of preferring Nickel's location. The file must exist.
bool writePosition(const QString &contentId, const QString &position, const NativeSnapshot &native, QString *error);

}
}

#endif
