// A DOM-like tree of one XHTML chapter, and the three position notations that
// meet in it:
//   - CREngine XPointers, as KOReader, Kobalt and CrossPoint exchange them:
//     "/body/DocFragment[3]/body/div[2]/p[5]/text().17"
//   - Kobo kepub spans, as Nickel stores them: "kobo.12.1" (element IDs)
//   - Adobe RMSDK points, as Nickel stores them for plain EPUB files:
//     "point(/1/4/2/14:0)" (child indices from the document element, UTF-8
//     byte offsets; the decoding follows Kobalt's engine_positions.cpp)
#ifndef KOSYNC_XPATH_H
#define KOSYNC_XPATH_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace kosync {

class XhtmlTree {
public:
    struct Node {
        int parent = -1;
        QVector<int> children;
        QString tag;   // lower-case local name (elements)
        QString id;    // id attribute (elements)
        QString text;  // character data (text nodes)
        bool element = false, isText = false, comment = false, cdata = false;
    };

    bool parse(const QByteArray &xml, QString *error);
    bool valid() const { return body_ >= 0; }
    const QVector<Node> &nodes() const { return nodes_; }
    int root() const { return root_; }
    int body() const { return body_; }

    // -- CREngine XPointers ------------------------------------------------
    // The XPointer of `charOffset` characters into text node `node`, or of the
    // start of element `node`. `node == body()` gives the chapter start.
    QString xpointer(int spineIndex, int node, int charOffset) const;
    // Resolves an XPointer inside this chapter (its DocFragment index must
    // already have selected the chapter). `node` is -1 for a chapter-level
    // pointer. Returns false when a step does not exist in this tree.
    bool resolve(const QString &xpointer, int *node, int *charOffset) const;

    // -- Kobo spans ---------------------------------------------------------
    int elementById(const QString &id) const;
    // The nearest kepub span ID ("kobo.N.M") for a position, or empty.
    QString koboSpanAt(int node) const;
    // The first text node inside `node` (or `node` itself), or -1.
    int firstTextIn(int node) const;

    // -- Adobe points ---------------------------------------------------------
    QString adobePoint(int node, int charOffset) const;
    bool resolveAdobePoint(const QString &point, int *node, int *charOffset) const;

    // Text of a node: its own characters, or all character data inside an element.
    QString textOf(int node) const;

private:
    int findDescendantWithKoboId(int node) const;
    QVector<Node> nodes_;
    int root_ = -1, body_ = -1;
};

// The DocFragment index (1-based) named by an XPointer, or -1.
int docFragmentIndex(const QString &xpointer);
// "/body[1]/DocFragment[3]/body[1]/..." -> "/body/DocFragment[3]/body/...":
// CREngine's newest DOM version writes explicit [1] indices that other readers
// (CrossPoint) do not parse. Everything after the inner body is kept.
QString normalizeXPointer(const QString &xpointer);
// The paragraph an XPointer points into: the pointer without its character
// offset, text-node step and kepub span steps. Two readers describing the
// same place produce different strings (one lands on the span, the other on
// the text inside it); they share this key.
QString paragraphKey(const QString &xpointer);
// Code point count of a UTF-16 prefix, and the reverse.
int codePoints(const QString &text, int utf16Length);
int utf16Length(const QString &text, int codePoints);

}

#endif
