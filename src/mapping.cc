#include "mapping.h"

#include "epub.h"
#include "log.h"
#include "nickeldb.h"
#include "xpath.h"

#include <QUrl>

namespace kosync {
namespace {

bool loadChapter(const Epub &epub, int spineIndex, XhtmlTree *tree, QString *error) {
    if (spineIndex < 0 || spineIndex >= epub.spine().size()) {
        *error = "chapter index outside the spine";
        return false;
    }
    const QByteArray xml = epub.read(epub.spine()[spineIndex], error);
    if (xml.isEmpty())
        return false;
    return tree->parse(xml, error);
}

}

bool xpointerFromNickel(const Epub &epub, const BookRow &row, QString *xpointer, QString *error) {
    xpointer->clear();
    const QString location = nickeldb::chapterLocation(row.contentId, row.chapter);
    if (location.isEmpty()) {
        *error = "Nickel has no position for this book";
        return false;
    }
    QString path, fragment;
    nickeldb::splitLocation(location, &path, &fragment);
    path = QUrl::fromPercentEncoding(path.toUtf8());
    const int spineIndex = epub.spineIndexOf(path);
    if (spineIndex < 0) {
        *error = "chapter is not in the spine: " + path;
        return false;
    }
    const QString chapterStart = "/body/DocFragment[" + QString::number(spineIndex + 1) + "].0";
    if (fragment.isEmpty()) {
        *xpointer = chapterStart;
        return true;
    }
    XhtmlTree tree;
    QString problem;
    if (!loadChapter(epub, spineIndex, &tree, &problem)) {
        debug("chapter " + path + " could not be parsed (" + problem + "); using the chapter start");
        *xpointer = chapterStart;
        return true;
    }
    int node = -1, offset = 0;
    if (fragment.startsWith("point(")) {
        if (!tree.resolveAdobePoint(fragment, &node, &offset)) {
            debug("Adobe point " + fragment + " not found in " + path + "; using the chapter start");
            *xpointer = chapterStart;
            return true;
        }
        if (!tree.nodes()[node].isText) {
            const int text = tree.firstTextIn(node);
            if (text >= 0) {
                node = text;
                offset = 0;
            }
        }
    } else {
        QString id = QUrl::fromPercentEncoding(fragment.toUtf8());
        const int colon = id.lastIndexOf(':');
        if (colon >= 0 && id.mid(colon + 1).toInt() >= 0 && !id.mid(colon + 1).isEmpty())
            id.truncate(colon);
        node = tree.elementById(id);
        if (node < 0) {
            debug("element " + id + " not found in " + path + "; using the chapter start");
            *xpointer = chapterStart;
            return true;
        }
        const int text = tree.firstTextIn(node);
        if (text >= 0)
            node = text;
        offset = 0;
    }
    *xpointer = tree.xpointer(spineIndex, node, offset);
    if (xpointer->isEmpty())
        *xpointer = chapterStart;
    return true;
}

bool nickelFromXPointer(const Epub &epub, const BookRow &row, const QString &xpointer, QString *location,
                        QString *error) {
    location->clear();
    const int fragment = docFragmentIndex(xpointer);
    if (fragment < 1 || fragment > epub.spine().size()) {
        *error = "the position names no chapter of this book: " + xpointer;
        return false;
    }
    const int spineIndex = fragment - 1;
    const QString path = epub.spine()[spineIndex];
    *location = path;
    XhtmlTree tree;
    QString problem;
    if (!loadChapter(epub, spineIndex, &tree, &problem)) {
        debug("chapter " + path + " could not be parsed (" + problem + "); storing the chapter start");
        return true;
    }
    int node = -1, offset = 0;
    if (!tree.resolve(xpointer, &node, &offset) || node < 0) {
        debug("position " + xpointer + " not found in " + path + "; storing the chapter start");
        return true;
    }
    if (row.kepub()) {
        const QString span = tree.koboSpanAt(node);
        if (!span.isEmpty())
            *location = path + '#' + span;
        else
            debug("no kepub span near " + xpointer + "; storing the chapter start");
    } else {
        const QString point = tree.adobePoint(node, offset);
        if (!point.isEmpty())
            *location = path + '#' + point;
    }
    return true;
}

double fractionOfXPointer(const Epub &epub, const QString &xpointer) {
    const int fragment = docFragmentIndex(xpointer);
    if (fragment < 1 || fragment > epub.spine().size())
        return 0;
    const int spineIndex = fragment - 1;
    double within = 0;
    XhtmlTree tree;
    QString error;
    int node = -1, offset = 0;
    if (loadChapter(epub, spineIndex, &tree, &error) && tree.resolve(xpointer, &node, &offset) && node > 0) {
        // Characters before the position over the chapter's characters.
        qint64 before = 0, total = 0;
        bool passed = false;
        const QVector<XhtmlTree::Node> &nodes = tree.nodes();
        QVector<int> pending;
        pending.append(tree.body());
        while (!pending.isEmpty()) {
            const int current = pending.takeLast();
            if (current == node)
                passed = true;
            const XhtmlTree::Node &n = nodes[current];
            if (n.isText) {
                total += n.text.size();
                if (!passed)
                    before += n.text.size();
                else if (current == node)
                    before += qMin<qint64>(n.text.size(), utf16Length(n.text, offset));
            } else {
                for (int i = n.children.size() - 1; i >= 0; --i)
                    pending.append(n.children[i]);
            }
        }
        if (total > 0)
            within = double(before) / double(total);
    }
    return epub.fraction(spineIndex, within);
}

}
