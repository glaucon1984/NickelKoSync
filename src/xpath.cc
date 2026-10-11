#include "xpath.h"

#include <QRegularExpression>
#include <QStringList>
#include <QXmlStreamEntityResolver>
#include <QXmlStreamReader>

namespace kosync {
namespace {

// HTML 4 named character references. Chapter files declare them through the
// XHTML DTD, which QXmlStreamReader does not fetch, so they are resolved here.
const char *const latin1Names[96] = {
    "nbsp",   "iexcl",  "cent",   "pound",  "curren", "yen",    "brvbar", "sect",   "uml",    "copy",   "ordf",
    "laquo",  "not",    "shy",    "reg",    "macr",   "deg",    "plusmn", "sup2",   "sup3",   "acute",  "micro",
    "para",   "middot", "cedil",  "sup1",   "ordm",   "raquo",  "frac14", "frac12", "frac34", "iquest", "Agrave",
    "Aacute", "Acirc",  "Atilde", "Auml",   "Aring",  "AElig",  "Ccedil", "Egrave", "Eacute", "Ecirc",  "Euml",
    "Igrave", "Iacute", "Icirc",  "Iuml",   "ETH",    "Ntilde", "Ograve", "Oacute", "Ocirc",  "Otilde", "Ouml",
    "times",  "Oslash", "Ugrave", "Uacute", "Ucirc",  "Uuml",   "Yacute", "THORN",  "szlig",  "agrave", "aacute",
    "acirc",  "atilde", "auml",   "aring",  "aelig",  "ccedil", "egrave", "eacute", "ecirc",  "euml",   "igrave",
    "iacute", "icirc",  "iuml",   "eth",    "ntilde", "ograve", "oacute", "ocirc",  "otilde", "ouml",   "divide",
    "oslash", "ugrave", "uacute", "ucirc",  "uuml",   "yacute", "thorn",  "yuml"};

struct NamedEntity {
    const char *name;
    int code;
};

const NamedEntity otherEntities[] = {
    {"quot", 34},      {"amp", 38},       {"lt", 60},        {"gt", 62},        {"apos", 39},      {"OElig", 338},
    {"oelig", 339},    {"Scaron", 352},   {"scaron", 353},   {"Yuml", 376},     {"fnof", 402},     {"circ", 710},
    {"tilde", 732},    {"Alpha", 913},    {"Beta", 914},     {"Gamma", 915},    {"Delta", 916},    {"Epsilon", 917},
    {"Zeta", 918},     {"Eta", 919},      {"Theta", 920},    {"Iota", 921},     {"Kappa", 922},    {"Lambda", 923},
    {"Mu", 924},       {"Nu", 925},       {"Xi", 926},       {"Omicron", 927},  {"Pi", 928},       {"Rho", 929},
    {"Sigma", 931},    {"Tau", 932},      {"Upsilon", 933},  {"Phi", 934},      {"Chi", 935},      {"Psi", 936},
    {"Omega", 937},    {"alpha", 945},    {"beta", 946},     {"gamma", 947},    {"delta", 948},    {"epsilon", 949},
    {"zeta", 950},     {"eta", 951},      {"theta", 952},    {"iota", 953},     {"kappa", 954},    {"lambda", 955},
    {"mu", 956},       {"nu", 957},       {"xi", 958},       {"omicron", 959},  {"pi", 960},       {"rho", 961},
    {"sigmaf", 962},   {"sigma", 963},    {"tau", 964},      {"upsilon", 965},  {"phi", 966},      {"chi", 967},
    {"psi", 968},      {"omega", 969},    {"thetasym", 977}, {"upsih", 978},    {"piv", 982},      {"ensp", 8194},
    {"emsp", 8195},    {"thinsp", 8201},  {"zwnj", 8204},    {"zwj", 8205},     {"lrm", 8206},     {"rlm", 8207},
    {"ndash", 8211},   {"mdash", 8212},   {"lsquo", 8216},   {"rsquo", 8217},   {"sbquo", 8218},   {"ldquo", 8220},
    {"rdquo", 8221},   {"bdquo", 8222},   {"dagger", 8224},  {"Dagger", 8225},  {"bull", 8226},    {"hellip", 8230},
    {"permil", 8240},  {"prime", 8242},   {"Prime", 8243},   {"lsaquo", 8249},  {"rsaquo", 8250},  {"oline", 8254},
    {"frasl", 8260},   {"euro", 8364},    {"image", 8465},   {"weierp", 8472},  {"real", 8476},    {"trade", 8482},
    {"alefsym", 8501}, {"larr", 8592},    {"uarr", 8593},    {"rarr", 8594},    {"darr", 8595},    {"harr", 8596},
    {"crarr", 8629},   {"lArr", 8656},    {"uArr", 8657},    {"rArr", 8658},    {"dArr", 8659},    {"hArr", 8660},
    {"forall", 8704},  {"part", 8706},    {"exist", 8707},   {"empty", 8709},   {"nabla", 8711},   {"isin", 8712},
    {"notin", 8713},   {"ni", 8715},      {"prod", 8719},    {"sum", 8721},     {"minus", 8722},   {"lowast", 8727},
    {"radic", 8730},   {"prop", 8733},    {"infin", 8734},   {"ang", 8736},     {"and", 8743},     {"or", 8744},
    {"cap", 8745},     {"cup", 8746},     {"int", 8747},     {"there4", 8756},  {"sim", 8764},     {"cong", 8773},
    {"asymp", 8776},   {"ne", 8800},      {"equiv", 8801},   {"le", 8804},      {"ge", 8805},      {"sub", 8834},
    {"sup", 8835},     {"nsub", 8836},    {"sube", 8838},    {"supe", 8839},    {"oplus", 8853},   {"otimes", 8855},
    {"perp", 8869},    {"sdot", 8901},    {"lceil", 8968},   {"rceil", 8969},   {"lfloor", 8970},  {"rfloor", 8971},
    {"lang", 9001},    {"rang", 9002},    {"loz", 9674},     {"spades", 9824},  {"clubs", 9827},   {"hearts", 9829},
    {"diams", 9830},   {0, 0}};

class HtmlEntityResolver : public QXmlStreamEntityResolver {
public:
    QString resolveUndeclaredEntity(const QString &name) {
        for (int i = 0; i < 96; ++i)
            if (name == QLatin1String(latin1Names[i]))
                return QString(QChar(160 + i));
        for (const NamedEntity *entity = otherEntities; entity->name; ++entity)
            if (name == QLatin1String(entity->name))
                return QString(QChar(entity->code));
        return QString();
    }
};

// CREngine's IsEmptySpace(): only these four characters are empty space.
bool whitespaceOnly(const QString &text) {
    for (int i = 0; i < text.size(); ++i) {
        const ushort c = text[i].unicode();
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t')
            return false;
    }
    return true;
}

struct Step {
    QString name;  // element name, or "text()"
    int index;     // 1-based; 0 when not given
};

// Splits the part of an XPointer after the inner body into steps and the
// trailing ".offset". Returns false on syntax it does not understand.
bool parseSteps(const QString &tail, QVector<Step> *steps, int *offset) {
    *offset = -1;
    QString path = tail;
    const int dot = path.lastIndexOf('.');
    if (dot >= 0 && dot > path.lastIndexOf('/')) {
        bool ok = false;
        *offset = path.mid(dot + 1).toInt(&ok);
        if (!ok || *offset < 0)
            return false;
        path.truncate(dot);
    }
    static const QRegularExpression stepFormat("^([A-Za-z_][A-Za-z0-9_:.-]*|text\\(\\))(?:\\[([0-9]+)\\])?$");
    foreach (const QString &part, path.split('/')) {
        if (part.isEmpty())
            continue;
        const QRegularExpressionMatch match = stepFormat.match(part);
        if (!match.hasMatch())
            return false;
        Step step;
        step.name = match.captured(1);
        step.index = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
        if (step.name != "text()")
            step.name = step.name.toLower();
        steps->append(step);
    }
    return true;
}

// "/body[1]/DocFragment[3]/body[1]/rest" -> rest ("" when none), -1 on mismatch.
QString fragmentTail(const QString &xpointer, int *fragment) {
    static const QRegularExpression head(
        "^/body(?:\\[1\\])?/DocFragment\\[([0-9]+)\\](?:\\.0|(?:/body(?:\\[1\\])?(.*)))?$");
    const QRegularExpressionMatch match = head.match(xpointer);
    *fragment = -1;
    if (!match.hasMatch())
        return QString();
    *fragment = match.captured(1).toInt();
    return match.captured(2);
}

}

QString paragraphKey(const QString &xpointer) {
    QString key = normalizeXPointer(xpointer);
    const int dot = key.lastIndexOf('.');
    if (dot > key.lastIndexOf('/'))
        key.truncate(dot);
    static const QRegularExpression trailing("(/text\\(\\)(?:\\[[0-9]+\\])?|/span(?:\\[[0-9]+\\])?)$");
    for (;;) {
        const QRegularExpressionMatch match = trailing.match(key);
        if (!match.hasMatch())
            break;
        key.truncate(match.capturedStart(0));
    }
    return key;
}

int codePoints(const QString &text, int utf16Length) {
    int count = 0;
    for (int i = 0; i < utf16Length && i < text.size(); ++i) {
        ++count;
        if (text[i].isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate())
            ++i;
    }
    return count;
}

int utf16Length(const QString &text, int codePoints) {
    int i = 0;
    for (int seen = 0; seen < codePoints && i < text.size(); ++seen) {
        if (text[i].isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate())
            ++i;
        ++i;
    }
    return i;
}

int docFragmentIndex(const QString &xpointer) {
    int fragment = -1;
    fragmentTail(xpointer, &fragment);
    return fragment;
}

QString normalizeXPointer(const QString &xpointer) {
    int fragment = -1;
    const QString tail = fragmentTail(xpointer, &fragment);
    if (fragment < 0)
        return xpointer;
    if (tail.isEmpty())
        return "/body/DocFragment[" + QString::number(fragment) + "].0";
    return "/body/DocFragment[" + QString::number(fragment) + "]/body" + tail;
}

bool XhtmlTree::parse(const QByteArray &xml, QString *error) {
    nodes_.clear();
    nodes_.append(Node());
    root_ = body_ = -1;
    QVector<int> stack;
    stack.append(0);
    HtmlEntityResolver resolver;
    QXmlStreamReader parser(xml);
    parser.setEntityResolver(&resolver);
    bool adjacentText = false;
    while (!parser.atEnd()) {
        parser.readNext();
        if (!parser.isCharacters())
            adjacentText = false;
        if (parser.isStartElement() || parser.isCharacters() || parser.isComment()) {
            const int parent = stack.last();
            if (parser.isCharacters() && adjacentText && !nodes_[parent].children.isEmpty()) {
                Node &last = nodes_[nodes_[parent].children.last()];
                if (last.isText && !parser.isCDATA() && !last.cdata) {
                    last.text += parser.text();
                    continue;
                }
            }
            Node node;
            node.parent = parent;
            node.isText = parser.isCharacters();
            node.cdata = parser.isCDATA();
            node.comment = parser.isComment();
            node.element = parser.isStartElement();
            if (node.element) {
                node.tag = parser.name().toString().toLower();
                node.id = parser.attributes().value("id").toString();
            }
            if (node.isText)
                node.text = parser.text().toString();
            adjacentText = node.isText;
            const int index = nodes_.size();
            nodes_.append(node);
            nodes_[parent].children.append(index);
            if (node.element) {
                if (parent == 0 && root_ < 0)
                    root_ = index;
                if (body_ < 0 && node.tag == "body")
                    body_ = index;
                stack.append(index);
            }
        } else if (parser.isEndElement()) {
            if (stack.size() > 1)
                stack.removeLast();
        }
    }
    if (parser.hasError()) {
        *error = parser.errorString();
        nodes_.clear();
        root_ = body_ = -1;
        return false;
    }
    if (root_ < 0 || body_ < 0) {
        *error = "no body element";
        return false;
    }
    return true;
}

QString XhtmlTree::textOf(int node) const {
    if (node < 0 || node >= nodes_.size())
        return QString();
    if (nodes_[node].isText)
        return nodes_[node].text;
    QString text;
    QVector<int> pending;
    pending.append(node);
    while (!pending.isEmpty()) {
        const int current = pending.takeLast();
        const Node &n = nodes_[current];
        if (n.isText)
            text += n.text;
        else
            for (int i = n.children.size() - 1; i >= 0; --i)
                pending.append(n.children[i]);
    }
    return text;
}

QString XhtmlTree::xpointer(int spineIndex, int node, int charOffset) const {
    const QString fragment = "/body/DocFragment[" + QString::number(spineIndex + 1) + "]";
    if (node < 0 || node >= nodes_.size() || node == body_)
        return fragment + ".0";
    QString path;
    int current = node;
    while (current > 0 && current != body_) {
        const Node &n = nodes_[current];
        const Node &parent = nodes_[n.parent];
        if (n.element) {
            int index = 0, count = 0;
            foreach (int sibling, parent.children) {
                if (nodes_[sibling].element && nodes_[sibling].tag == n.tag) {
                    ++count;
                    if (sibling == current)
                        index = count;
                }
            }
            path = '/' + n.tag + '[' + QString::number(index) + ']' + path;
        } else if (n.isText) {
            int index = 0, count = 0;
            foreach (int sibling, parent.children) {
                if (nodes_[sibling].isText && (sibling == current || !whitespaceOnly(nodes_[sibling].text))) {
                    ++count;
                    if (sibling == current)
                        index = count;
                }
            }
            path = count > 1 ? "/text()[" + QString::number(index) + ']' + path : "/text()" + path;
        } else {
            return QString();  // a comment has no XPointer
        }
        current = n.parent;
    }
    if (current != body_)
        return QString();
    return fragment + "/body" + path + '.' + QString::number(qMax(0, charOffset));
}

bool XhtmlTree::resolve(const QString &xpointer, int *node, int *charOffset) const {
    *node = -1;
    *charOffset = 0;
    int fragment = -1;
    const QString tail = fragmentTail(xpointer, &fragment);
    if (fragment < 0 || body_ < 0)
        return false;
    if (tail.isEmpty())
        return true;  // chapter level
    QVector<Step> steps;
    int offset = -1;
    if (!parseSteps(tail, &steps, &offset))
        return false;
    int current = body_;
    foreach (const Step &step, steps) {
        const Node &parent = nodes_[current];
        const int wanted = step.index > 0 ? step.index : 1;
        int seen = 0, found = -1;
        if (step.name == "text()") {
            foreach (int child, parent.children) {
                if (nodes_[child].isText && !whitespaceOnly(nodes_[child].text) && ++seen == wanted) {
                    found = child;
                    break;
                }
            }
        } else {
            foreach (int child, parent.children) {
                if (nodes_[child].element && nodes_[child].tag == step.name && ++seen == wanted) {
                    found = child;
                    break;
                }
            }
        }
        if (found < 0)
            return false;
        current = found;
    }
    *node = current;
    if (nodes_[current].isText && offset > 0)
        *charOffset = qMin(offset, codePoints(nodes_[current].text, nodes_[current].text.size()));
    return true;
}

int XhtmlTree::elementById(const QString &id) const {
    if (id.isEmpty())
        return -1;
    for (int i = 1; i < nodes_.size(); ++i)
        if (nodes_[i].element && nodes_[i].id == id)
            return i;
    return -1;
}

int XhtmlTree::findDescendantWithKoboId(int node) const {
    QVector<int> pending;
    pending.append(node);
    while (!pending.isEmpty()) {
        const int current = pending.takeFirst();
        const Node &n = nodes_[current];
        if (n.element && n.id.startsWith("kobo.") && current != node)
            return current;
        foreach (int child, n.children)
            if (nodes_[child].element)
                pending.append(child);
    }
    return -1;
}

QString XhtmlTree::koboSpanAt(int node) const {
    if (node < 0 || node >= nodes_.size())
        return QString();
    for (int current = node; current > 0; current = nodes_[current].parent)
        if (nodes_[current].element && nodes_[current].id.startsWith("kobo."))
            return nodes_[current].id;
    const int inside = findDescendantWithKoboId(nodes_[node].isText ? nodes_[node].parent : node);
    return inside < 0 ? QString() : nodes_[inside].id;
}

int XhtmlTree::firstTextIn(int node) const {
    if (node < 0 || node >= nodes_.size())
        return -1;
    if (nodes_[node].isText)
        return node;
    QVector<int> pending;
    pending.append(node);
    while (!pending.isEmpty()) {
        const int current = pending.takeLast();
        const Node &n = nodes_[current];
        if (n.isText && !whitespaceOnly(n.text))
            return current;
        for (int i = n.children.size() - 1; i >= 0; --i)
            pending.append(n.children[i]);
    }
    return -1;
}

QString XhtmlTree::adobePoint(int node, int charOffset) const {
    if (node <= 0 || node >= nodes_.size() || root_ < 0)
        return QString();
    QString path;
    int current = node;
    while (current != root_) {
        const Node &n = nodes_[current];
        if (n.parent < 0)
            return QString();
        const int index = nodes_[n.parent].children.indexOf(current);
        if (index < 0)
            return QString();
        path = '/' + QString::number(index + 1) + path;
        current = n.parent;
    }
    QString point = "point(/1" + path;
    if (nodes_[node].isText && charOffset > 0) {
        const QString &text = nodes_[node].text;
        const int bytes = text.left(utf16Length(text, charOffset)).toUtf8().size();
        point += ':' + QString::number(bytes);
    }
    return point + ')';
}

bool XhtmlTree::resolveAdobePoint(const QString &point, int *node, int *charOffset) const {
    *node = -1;
    *charOffset = 0;
    static const QRegularExpression format("^point\\((/1(?:/[1-9][0-9]*)*)(?::([0-9]+))?\\)$");
    const QRegularExpressionMatch match = format.match(point);
    if (!match.hasMatch() || root_ < 0)
        return false;
    int target = root_;
    const QStringList sequence = match.captured(1).split('/');
    for (int i = 2; i < sequence.size(); ++i) {
        const int child = sequence[i].toInt() - 1;
        if (child < 0 || child >= nodes_[target].children.size())
            return false;
        target = nodes_[target].children[child];
    }
    if (!nodes_[target].isText && !nodes_[target].element)
        return false;
    *node = target;
    const int bytes = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    if (bytes > 0) {
        const QString text = textOf(target);
        const QByteArray utf8 = text.toUtf8();
        if (bytes > utf8.size())
            return false;
        const QString prefix = QString::fromUtf8(utf8.constData(), bytes);
        *charOffset = codePoints(prefix, prefix.size());
        if (!nodes_[target].isText) {
            // Walk into the text node that holds the offset.
            int remaining = *charOffset;
            QVector<int> pending;
            pending.append(target);
            while (!pending.isEmpty()) {
                const int current = pending.takeLast();
                const Node &n = nodes_[current];
                if (n.isText) {
                    const int length = codePoints(n.text, n.text.size());
                    if (remaining <= length) {
                        *node = current;
                        *charOffset = remaining;
                        return true;
                    }
                    remaining -= length;
                } else {
                    for (int i = n.children.size() - 1; i >= 0; --i)
                        pending.append(n.children[i]);
                }
            }
            *charOffset = 0;
        }
    }
    return true;
}

}
