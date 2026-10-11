#include "digest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace kosync {

QString partialMd5(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    const qint64 size = file.size();
    QCryptographicHash md5(QCryptographicHash::Md5);
    for (int i = -1; i <= 10; ++i) {
        const qint64 offset = i < 0 ? 0 : (qint64(1024) << (2 * i));
        if (offset >= size)
            break;
        if (!file.seek(offset))
            return QString();
        const QByteArray sample = file.read(1024);
        if (sample.isEmpty())
            break;
        md5.addData(sample);
    }
    return QString::fromLatin1(md5.result().toHex());
}

QString filenameMd5(const QString &path) {
    const QString name = QFileInfo(path).fileName();
    if (name.isEmpty())
        return QString();
    return QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Md5).toHex());
}

}
