#include "epub.h"

#include <zlib.h>

#include <cstring>

#include <QFile>
#include <QUrl>
#include <QXmlStreamReader>

namespace kosync {
namespace {

quint16 le16(const uchar *p) {
    return quint16(p[0] | (p[1] << 8));
}
quint32 le32(const uchar *p) {
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

QString normalizeEntry(const QString &name) {
    QString value = QUrl::fromPercentEncoding(name.toUtf8());
    while (value.startsWith('/'))
        value.remove(0, 1);
    return value;
}

}

QString resolvePath(const QString &base, const QString &relative) {
    QString directory = base.left(base.lastIndexOf('/') + 1);
    QString target = relative;
    if (target.startsWith('/')) {
        directory.clear();
        target.remove(0, 1);
    }
    QStringList parts;
    foreach (const QString &part, (directory + target).split('/')) {
        if (part.isEmpty() || part == ".")
            continue;
        if (part == "..") {
            if (!parts.isEmpty())
                parts.removeLast();
            continue;
        }
        parts.append(part);
    }
    return parts.join("/");
}

bool Epub::open(const QString &path, QString *error) {
    path_ = path;
    entries_.clear();
    spine_.clear();
    title_.clear();
    author_.clear();
    if (!readDirectory(error))
        return false;
    return readOpf(error);
}

bool Epub::readDirectory(QString *error) {
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = "cannot open " + path_;
        return false;
    }
    const qint64 size = file.size();
    const qint64 tail = qMin<qint64>(size, 65536 + 22);
    if (!file.seek(size - tail)) {
        *error = "cannot seek to the ZIP directory";
        return false;
    }
    const QByteArray end = file.read(tail);
    int eocd = -1;
    for (int i = end.size() - 22; i >= 0; --i) {
        if (le32(reinterpret_cast<const uchar *>(end.constData() + i)) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        *error = "not a ZIP file";
        return false;
    }
    const uchar *e = reinterpret_cast<const uchar *>(end.constData() + eocd);
    const quint16 count = le16(e + 10);
    const quint32 directorySize = le32(e + 12), directoryOffset = le32(e + 16);
    if (directoryOffset == 0xffffffff || count == 0xffff) {
        *error = "ZIP64 archives are not supported";
        return false;
    }
    if (!file.seek(directoryOffset)) {
        *error = "cannot seek to the ZIP directory";
        return false;
    }
    const QByteArray directory = file.read(directorySize);
    if (quint32(directory.size()) != directorySize) {
        *error = "truncated ZIP directory";
        return false;
    }
    const uchar *d = reinterpret_cast<const uchar *>(directory.constData());
    quint32 pos = 0;
    for (quint16 i = 0; i < count; ++i) {
        if (pos + 46 > directorySize || le32(d + pos) != 0x02014b50) {
            *error = "damaged ZIP directory";
            return false;
        }
        Entry entry;
        entry.method = le16(d + pos + 10);
        entry.compressed = le32(d + pos + 20);
        entry.uncompressed = le32(d + pos + 24);
        const quint16 nameLength = le16(d + pos + 28), extraLength = le16(d + pos + 30),
                      commentLength = le16(d + pos + 32);
        entry.offset = le32(d + pos + 42);
        if (pos + 46 + nameLength > directorySize) {
            *error = "damaged ZIP directory";
            return false;
        }
        const QString name = QString::fromUtf8(directory.constData() + pos + 46, nameLength);
        if (!name.endsWith('/'))
            entries_.insert(normalizeEntry(name), entry);
        pos += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

qint64 Epub::sizeOf(const QString &name) const {
    const QHash<QString, Entry>::const_iterator it = entries_.find(name);
    return it == entries_.end() ? -1 : qint64(it->uncompressed);
}

QByteArray Epub::read(const QString &name, QString *error) const {
    const QHash<QString, Entry>::const_iterator it = entries_.find(name);
    if (it == entries_.end()) {
        *error = "no such entry: " + name;
        return QByteArray();
    }
    const Entry &entry = *it;
    if (entry.uncompressed > maxEntryBytes || entry.compressed > maxEntryBytes) {
        *error = "entry too large: " + name;
        return QByteArray();
    }
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(entry.offset)) {
        *error = "cannot read " + path_;
        return QByteArray();
    }
    const QByteArray header = file.read(30);
    if (header.size() != 30 || le32(reinterpret_cast<const uchar *>(header.constData())) != 0x04034b50) {
        *error = "damaged ZIP entry: " + name;
        return QByteArray();
    }
    const uchar *h = reinterpret_cast<const uchar *>(header.constData());
    const quint16 nameLength = le16(h + 26), extraLength = le16(h + 28);
    if (!file.seek(entry.offset + 30 + nameLength + extraLength)) {
        *error = "damaged ZIP entry: " + name;
        return QByteArray();
    }
    const QByteArray compressed = file.read(entry.compressed);
    if (quint32(compressed.size()) != entry.compressed) {
        *error = "truncated ZIP entry: " + name;
        return QByteArray();
    }
    if (entry.method == 0)
        return compressed;
    if (entry.method != 8) {
        *error = "unsupported compression method in " + name;
        return QByteArray();
    }
    QByteArray output(int(entry.uncompressed), '\0');
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        *error = "zlib initialisation failed";
        return QByteArray();
    }
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = uInt(compressed.size());
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = uInt(output.size());
    const int result = inflate(&stream, Z_FINISH);
    const uInt produced = uInt(output.size()) - stream.avail_out;
    inflateEnd(&stream);
    if (result != Z_STREAM_END && !(result == Z_BUF_ERROR && produced == uInt(output.size()))) {
        *error = "inflate failed for " + name;
        return QByteArray();
    }
    output.resize(int(produced));
    return output;
}

bool Epub::readOpf(QString *error) {
    QString problem;
    const QByteArray container = read("META-INF/container.xml", &problem);
    if (container.isEmpty()) {
        *error = "no META-INF/container.xml (" + problem + ")";
        return false;
    }
    QString opfPath;
    {
        QXmlStreamReader xml(container);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == "rootfile") {
                const QString full = xml.attributes().value("full-path").toString();
                if (!full.isEmpty() && opfPath.isEmpty())
                    opfPath = normalizeEntry(full);
            }
        }
    }
    if (opfPath.isEmpty()) {
        *error = "container.xml names no OPF file";
        return false;
    }
    const QByteArray opf = read(opfPath, &problem);
    if (opf.isEmpty()) {
        *error = "cannot read the OPF file (" + problem + ")";
        return false;
    }
    QHash<QString, QString> manifest;  // id -> entry name
    QStringList refs;
    QXmlStreamReader xml(opf);
    QString text;
    bool inTitle = false, inCreator = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringRef name = xml.name();
            if (name == "item") {
                const QString id = xml.attributes().value("id").toString();
                const QString href = xml.attributes().value("href").toString();
                if (!id.isEmpty() && !href.isEmpty())
                    manifest.insert(id, resolvePath(opfPath, QUrl::fromPercentEncoding(href.toUtf8())));
            } else if (name == "itemref") {
                refs.append(xml.attributes().value("idref").toString());
            } else if (name == "title" && title_.isEmpty()) {
                inTitle = true;
                text.clear();
            } else if (name == "creator" && author_.isEmpty()) {
                inCreator = true;
                text.clear();
            }
        } else if (xml.isCharacters() && (inTitle || inCreator)) {
            text += xml.text();
        } else if (xml.isEndElement()) {
            if (inTitle && xml.name() == "title") {
                title_ = text.simplified();
                inTitle = false;
            } else if (inCreator && xml.name() == "creator") {
                author_ = text.simplified();
                inCreator = false;
            }
        }
    }
    if (xml.hasError()) {
        *error = "cannot parse the OPF file: " + xml.errorString();
        return false;
    }
    foreach (const QString &ref, refs) {
        const QHash<QString, QString>::const_iterator it = manifest.find(ref);
        if (it != manifest.end())
            spine_.append(*it);
    }
    if (spine_.isEmpty()) {
        *error = "the OPF spine is empty";
        return false;
    }
    return true;
}

int Epub::spineIndexOf(const QString &chapterPath) const {
    const QString wanted = normalizeEntry(chapterPath);
    for (int i = 0; i < spine_.size(); ++i)
        if (spine_[i] == wanted)
            return i;
    // Nickel may use a path relative to the OPF; accept a unique suffix match.
    int found = -1;
    for (int i = 0; i < spine_.size(); ++i) {
        if (spine_[i].endsWith('/' + wanted)) {
            if (found >= 0)
                return -1;
            found = i;
        }
    }
    return found;
}

double Epub::fraction(int spineIndex, double within) const {
    if (spine_.isEmpty())
        return 0;
    qint64 total = 0, before = 0, current = 0;
    for (int i = 0; i < spine_.size(); ++i) {
        const qint64 size = qMax<qint64>(1, sizeOf(spine_[i]));
        total += size;
        if (i < spineIndex)
            before += size;
        else if (i == spineIndex)
            current = size;
    }
    if (total <= 0)
        return 0;
    const double value = (before + qBound(0.0, within, 1.0) * current) / double(total);
    return qBound(0.0, value, 1.0);
}

}
