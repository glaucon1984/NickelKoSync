// Minimal EPUB access: the ZIP directory, inflating single entries, and the
// OPF spine. Enough to find a chapter file and estimate a position in the book.
#ifndef KOSYNC_EPUB_H
#define KOSYNC_EPUB_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

namespace kosync {

class Epub {
public:
    static const qint64 maxEntryBytes = 16 * 1024 * 1024;

    bool open(const QString &path, QString *error);
    const QString &path() const { return path_; }

    // Entry names in spine order, as stored in the ZIP (percent-decoded,
    // relative to the archive root).
    const QStringList &spine() const { return spine_; }
    // Index in spine() of a chapter given by a path relative to the archive
    // root (Nickel's chapter paths), or -1.
    int spineIndexOf(const QString &chapterPath) const;
    bool hasEntry(const QString &name) const { return entries_.contains(name); }
    qint64 sizeOf(const QString &name) const;
    QByteArray read(const QString &name, QString *error) const;

    // Position in the book as a fraction (0..1) from the spine item and the
    // fraction read within it, weighting chapters by their uncompressed size.
    double fraction(int spineIndex, double within) const;

    const QString &title() const { return title_; }
    const QString &author() const { return author_; }

private:
    struct Entry {
        quint16 method = 0;
        quint32 compressed = 0, uncompressed = 0;
        quint32 offset = 0;
    };
    bool readDirectory(QString *error);
    bool readOpf(QString *error);

    QString path_, title_, author_;
    QHash<QString, Entry> entries_;
    QStringList spine_;
};

// Joins `relative` to the directory of `base` and normalizes "." and "..".
QString resolvePath(const QString &base, const QString &relative);

}

#endif
