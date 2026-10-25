// Host tests for the Qt-only units: XHTML positions, EPUB access, digests,
// Kobalt's saved state. Build and run with `make test` (needs a host Qt 5 with
// the SQLite driver; see tools/host-test.sh).
#include "digest.h"
#include "epub.h"
#include "kobalt_state.h"
#include "log.h"
#include "nickeldb.h"
#include "xpath.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>

#include <cstdio>

using namespace kosync;

// kobalt_state.cc reports through log.h, which goes to NickelHook on the
// device; on the host, print instead.
namespace kosync {
static bool verboseLog = false;
void setVerboseLogging(bool on) { verboseLog = on; }
bool verboseLogging() { return verboseLog; }
void log(const QString &message) { std::fprintf(stderr, "log: %s\n", message.toUtf8().constData()); }
void debug(const QString &message) { if (verboseLog) log(message); }
}

static int failures = 0;

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            ++failures;                                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
        }                                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                        \
    do {                                                                                                  \
        const QString a_ = QString(actual), e_ = QString(expected);                                       \
        if (a_ != e_) {                                                                                   \
            ++failures;                                                                                   \
            std::fprintf(stderr, "FAIL %s:%d: %s\n  got      '%s'\n  expected '%s'\n", __FILE__, __LINE__, \
                         #actual, a_.toUtf8().constData(), e_.toUtf8().constData());                      \
        }                                                                                                 \
    } while (0)

static void testTree(const QByteArray &xml) {
    XhtmlTree tree;
    QString error;
    CHECK(tree.parse(xml, &error));
    if (!tree.valid()) {
        std::fprintf(stderr, "parse error: %s\n", error.toUtf8().constData());
        return;
    }
    // entities from the XHTML DTD are resolved
    const int heading = tree.elementById("kobo.1.1");
    CHECK(heading > 0);
    CHECK(tree.textOf(heading).contains(QChar(0xa0)));
    const int third = tree.elementById("kobo.3.1");
    CHECK(tree.textOf(third).contains(QChar(0x2014)));

    // kepub span -> XPointer
    const int span = tree.elementById("kobo.2.2");
    CHECK(span > 0);
    const int text = tree.firstTextIn(span);
    CHECK(text > 0 && tree.nodes()[text].isText);
    const QString pointer = tree.xpointer(4, text, 0);
    CHECK_EQ(pointer, "/body/DocFragment[5]/body/div[1]/p[1]/span[2]/text().0");

    // XPointer -> node, in the three spellings that exist in the wild
    int node = -1, offset = 0;
    CHECK(tree.resolve(pointer, &node, &offset) && node == text && offset == 0);
    CHECK(tree.resolve("/body/DocFragment[5]/body/div/p/span[2]/text().3", &node, &offset) && node == text && offset == 3);
    CHECK(tree.resolve("/body[1]/DocFragment[5]/body[1]/div[1]/p[1]/span[2]/text()[1].3", &node, &offset) &&
          node == text && offset == 3);
    CHECK(tree.resolve("/body/DocFragment[5]/body/div[1]/p[1]/span[2].0", &node, &offset) && node == span);
    CHECK(tree.resolve("/body/DocFragment[5].0", &node, &offset) && node == -1);
    CHECK(tree.resolve("/body/DocFragment[5]/body/div[1]/p[9]/text().0", &node, &offset) == false);
    CHECK(tree.resolve("/body/DocFragment[5]/body/nosuch/text().0", &node, &offset) == false);

    // node -> kepub span
    CHECK_EQ(tree.koboSpanAt(text), "kobo.2.2");
    CHECK_EQ(tree.koboSpanAt(span), "kobo.2.2");
    const int paragraph = tree.nodes()[span].parent;
    CHECK_EQ(tree.koboSpanAt(paragraph), "kobo.2.1");

    // several text nodes under one element
    int lastP = -1;
    for (int i = 1; i < tree.nodes().size(); ++i)
        if (tree.nodes()[i].element && tree.nodes()[i].tag == "p")
            lastP = i;
    CHECK(lastP > 0);
    int tail = -1;
    foreach (int child, tree.nodes()[lastP].children)
        if (tree.nodes()[child].isText && tree.nodes()[child].text.contains("tail"))
            tail = child;
    CHECK(tail > 0);
    CHECK_EQ(tree.xpointer(0, tail, 1), "/body/DocFragment[1]/body/div[1]/p[3]/text()[2].1");
    CHECK(tree.resolve("/body/DocFragment[1]/body/div[1]/p[3]/text()[2].1", &node, &offset) && node == tail && offset == 1);
    // a comment has no XPointer
    CHECK(tree.koboSpanAt(tail).isEmpty());

    // Adobe points round-trip
    const int thirdText = tree.firstTextIn(third);
    const QString point = tree.adobePoint(thirdText, 6);
    CHECK(point.startsWith("point(/1/") && point.endsWith(":6)"));
    CHECK(tree.resolveAdobePoint(point, &node, &offset) && node == thirdText && offset == 6);
    const QString elementPoint = tree.adobePoint(third, 0);
    CHECK(!elementPoint.contains(':'));
    CHECK(tree.resolveAdobePoint(elementPoint, &node, &offset) && node == third && offset == 0);
    // an element point with a byte offset lands in the text inside it
    QString inner = elementPoint;
    inner.chop(1);
    inner += ":8)";
    CHECK(tree.resolveAdobePoint(inner, &node, &offset) && node == thirdText && offset == 8);
    CHECK(tree.resolveAdobePoint("point(/1/99)", &node, &offset) == false);

    // chapter start and helpers
    CHECK_EQ(tree.xpointer(2, tree.body(), 0), "/body/DocFragment[3].0");
    CHECK(docFragmentIndex("/body/DocFragment[7]/body/p[2]/text().4") == 7);
    CHECK(docFragmentIndex("/body[1]/DocFragment[7].0") == 7);
    CHECK(docFragmentIndex("garbage") == -1);
    CHECK_EQ(normalizeXPointer("/body[1]/DocFragment[5]/body[1]/div[1]/p[1].0"), "/body/DocFragment[5]/body/div[1]/p[1].0");
    CHECK_EQ(normalizeXPointer("/body/DocFragment[2].0"), "/body/DocFragment[2].0");
    CHECK_EQ(normalizeXPointer("/body[1]/DocFragment[2]"), "/body/DocFragment[2].0");
    CHECK_EQ(normalizeXPointer("/body/DocFragment[5]/body/div/p/text().3"), "/body/DocFragment[5]/body/div/p/text().3");
    // the same paragraph seen from Kobalt (span), KOReader (text) and CrossPoint
    CHECK_EQ(paragraphKey("/body/DocFragment[5]/body/div[1]/p[1]/span[2]/text().0"), "/body/DocFragment[5]/body/div[1]/p[1]");
    CHECK_EQ(paragraphKey("/body[1]/DocFragment[5]/body[1]/div[1]/p[1]/text()[1].17"), "/body/DocFragment[5]/body/div[1]/p[1]");
    CHECK_EQ(paragraphKey("/body/DocFragment[5]/body/div[1]/p[1].0"), "/body/DocFragment[5]/body/div[1]/p[1]");
    CHECK_EQ(paragraphKey("/body/DocFragment[5].0"), "/body/DocFragment[5]");
    CHECK(paragraphKey("/body/DocFragment[5]/body/div[1]/p[2]/text().0") != paragraphKey("/body/DocFragment[5]/body/div[1]/p[1]/text().0"));
    const QString emoji = QString::fromUtf8("a\xF0\x9F\x98\x80") + "b";  // a, U+1F600, b
    CHECK(codePoints(emoji, 4) == 3);
    CHECK(utf16Length(emoji, 2) == 3);
}

static void testEpub(const QString &path) {
    Epub epub;
    QString error;
    if (!epub.open(path, &error)) {
        ++failures;
        std::fprintf(stderr, "FAIL cannot open %s: %s\n", path.toUtf8().constData(), error.toUtf8().constData());
        return;
    }
    CHECK_EQ(epub.title(), "Fixture Book");
    CHECK_EQ(epub.author(), "Test Author");
    CHECK(epub.spine().size() == 3);
    if (epub.spine().size() == 3) {
        CHECK_EQ(epub.spine()[0], "OEBPS/Text/ch1.xhtml");
        CHECK_EQ(epub.spine()[1], "OEBPS/Text/ch 2.xhtml");
        CHECK_EQ(epub.spine()[2], "extra/ch3.xhtml");
    }
    CHECK(epub.spineIndexOf("OEBPS/Text/ch%202.xhtml") == 1);
    CHECK(epub.spineIndexOf("OEBPS/Text/ch 2.xhtml") == 1);
    CHECK(epub.spineIndexOf("Text/ch1.xhtml") == 0);  // relative to the OPF
    CHECK(epub.spineIndexOf("nowhere.xhtml") == -1);
    const QByteArray chapter = epub.read("OEBPS/Text/ch1.xhtml", &error);
    CHECK(chapter.contains("<title>One</title>"));
    CHECK(epub.read("mimetype", &error) == "application/epub+zip");
    CHECK(epub.read("missing", &error).isEmpty());
    CHECK(epub.fraction(0, 0) == 0);
    CHECK(epub.fraction(2, 1.0) == 1.0);
    CHECK(epub.fraction(1, 0.5) > epub.fraction(0, 1.0));
    CHECK(epub.fraction(1, 0.5) < epub.fraction(2, 0.0));
    testTree(chapter);

    QFile md5(path + ".md5");
    CHECK(md5.open(QIODevice::ReadOnly));
    QTextStream in(&md5);
    const QString partial = in.readLine(), byName = in.readLine();
    CHECK_EQ(partialMd5(path), partial);
    CHECK_EQ(filenameMd5(path), byName);
}

// Kobalt's book state: a book_state row in kobalt.sqlite (v1.0-beta.4, the
// table as Kobalt creates it) and the legacy per-book JSON file.
static void testKobaltState() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    kobalt::setDirectory(dir.path());
    const QString book = "file:///mnt/onboard/Books/Test - Author.epub";
    const QString other = "file:///mnt/onboard/Books/Other.epub";
    CHECK(!kobalt::hasDatabase());
    CHECK(!kobalt::readState(book).exists);

    const QByteArray payload =
        "{\"book\":\"" + book.toUtf8() + "\",\"schema\":1,\"fingerprint\":\"abc\",\"percent\":5,"
        "\"native_writeback\":\"verified\",\"sync_pending\":false,\"finished_position\":\"\","
        "\"position\":\"/body[1]/DocFragment[6]/body[1]/p[76]/text()[1].578\","
        "\"nickel_progress\":{\"book\":\"" + book.toUtf8() + "\",\"location\":\"OEBPS/c2H.xhtml\","
        "\"modified\":\"1792923707000\",\"status\":1,\"valid\":true}}";
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "fixture");
        db.setDatabaseName(kobalt::databasePath());
        CHECK(db.open());
        QSqlQuery query(db);
        CHECK(query.exec("CREATE TABLE book_state(identity TEXT PRIMARY KEY NOT NULL,format INTEGER NOT NULL CHECK(format>0),"
                         "revision INTEGER NOT NULL DEFAULT 0 CHECK(revision>=0),payload TEXT NOT NULL "
                         "CHECK(length(CAST(payload AS BLOB))<=16384))"));
        // Kobalt binds the identity and the payload as UTF-8 bytes cast to text.
        CHECK(query.prepare("INSERT INTO book_state(payload,identity,format,revision) VALUES(CAST(? AS TEXT),CAST(? AS TEXT),1,3)"));
        query.addBindValue(payload);
        query.addBindValue(book.toUtf8());
        CHECK(query.exec());
        db.close();
    }
    QSqlDatabase::removeDatabase("fixture");
    CHECK(kobalt::hasDatabase());

    kobalt::State state = kobalt::readState(book);
    CHECK(state.exists);
    CHECK_EQ(state.position, "/body[1]/DocFragment[6]/body[1]/p[76]/text()[1].578");
    CHECK(state.percent == 5);
    CHECK(state.modifiedMs == 0);
    CHECK_EQ(state.nativeLocation, "OEBPS/c2H.xhtml");
    CHECK(state.nativeModifiedMs == Q_INT64_C(1792923707000));
    CHECK_EQ(state.raw.value("native_writeback").toString(), "verified");
    CHECK(!kobalt::readState(other).exists);

    NativeSnapshot snapshot;
    snapshot.book = book;
    snapshot.location = "OEBPS/c2H.xhtml#point(/1/4/154/1)";
    snapshot.modifiedMs = Q_INT64_C(1792930000000);
    snapshot.status = 1;
    snapshot.valid = true;
    QString error;
    CHECK(kobalt::writePosition(book, "/body/DocFragment[6]/body/p[80]/text().0", snapshot, &error));
    CHECK_EQ(error, "");
    state = kobalt::readState(book);
    CHECK(state.exists);
    CHECK_EQ(state.position, "/body/DocFragment[6]/body/p[80]/text().0");
    CHECK_EQ(state.nativeLocation, snapshot.location);
    CHECK(state.nativeModifiedMs == snapshot.modifiedMs);
    CHECK(state.percent == 5);  // Kobalt's other fields are kept
    CHECK_EQ(state.raw.value("fingerprint").toString(), "abc");
    CHECK(state.raw.value("schema").toInt() == 1);
    CHECK(state.raw.value("sync_pending").toBool() == false);
    CHECK(!kobalt::writePosition(other, "/body", snapshot, &error));  // no record anywhere
    CHECK(!error.isEmpty());
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "fixture");
        db.setDatabaseName(kobalt::databasePath());
        CHECK(db.open());
        QSqlQuery query(db);
        CHECK(query.exec("SELECT revision, format, CAST(payload AS BLOB), identity FROM book_state"));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 4);  // one change, one more revision
        CHECK(query.value(1).toInt() == 1);
        const QByteArray stored = query.value(2).toByteArray();
        CHECK(!stored.contains('\n'));  // compact, as Kobalt writes it
        CHECK(QJsonDocument::fromJson(stored).object().value("book").toString() == book);
        CHECK_EQ(query.value(3).toString(), book);
        CHECK(!query.next());
        db.close();
    }
    QSqlDatabase::removeDatabase("fixture");

    // A legacy file for a book the database has no row for (not imported yet,
    // or Kobalt before beta 4).
    {
        QFile file(kobalt::statePath(other));
        CHECK(file.open(QIODevice::WriteOnly));
        file.write("{\"book\":\"" + other.toUtf8() + "\",\"schema\":1,\"position\":\"/body/DocFragment[2].0\","
                   "\"native_writeback\":\"time-only\"}");
    }
    state = kobalt::readState(other);
    CHECK(state.exists);
    CHECK_EQ(state.position, "/body/DocFragment[2].0");
    CHECK(state.percent == -1);
    CHECK(state.modifiedMs > 0);
    snapshot.book = other;
    CHECK(kobalt::writePosition(other, "/body/DocFragment[3]/body/p[1]/text().4", snapshot, &error));
    state = kobalt::readState(other);
    CHECK_EQ(state.position, "/body/DocFragment[3]/body/p[1]/text().4");
    CHECK_EQ(state.nativeLocation, snapshot.location);
    CHECK_EQ(state.raw.value("native_writeback").toString(), "time-only");
    // The database row still wins over a leftover file for the first book.
    CHECK_EQ(kobalt::readState(book).position, "/body/DocFragment[6]/body/p[80]/text().0");
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString fixture = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString("tests/fixture.epub");
    testEpub(fixture);
    testKobaltState();
    CHECK(resolvePath("OEBPS/content.opf", "Text/ch1.xhtml") == "OEBPS/Text/ch1.xhtml");
    CHECK(resolvePath("OEBPS/content.opf", "../extra/ch3.xhtml") == "extra/ch3.xhtml");
    CHECK(resolvePath("content.opf", "./a/./b.xhtml") == "a/b.xhtml");
    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all checks passed\n");
    return 0;
}
