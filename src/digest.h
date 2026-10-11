// KOReader's document identifiers for the sync server.
#ifndef KOSYNC_DIGEST_H
#define KOSYNC_DIGEST_H

#include <QString>

namespace kosync {

// KOReader's "binary" method: MD5 over 1 KiB samples taken at offsets 0 and
// 1024 << 2i for i = 0..10 (the "partial MD5"). Empty on failure.
QString partialMd5(const QString &path);

// KOReader's "filename" method: MD5 of the file's base name.
QString filenameMd5(const QString &path);

}

#endif
