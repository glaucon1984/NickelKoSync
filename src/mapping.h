// Translating Nickel's stored reading positions to CREngine XPointers and back,
// through the chapter's XHTML.
#ifndef KOSYNC_MAPPING_H
#define KOSYNC_MAPPING_H

#include <QString>

namespace kosync {

class Epub;
struct BookRow;

// The XPointer for Nickel's ChapterIDBookmarked of `row`. Falls back to the
// chapter start when the element inside the chapter cannot be located, and
// fails only when the chapter itself is unknown.
bool xpointerFromNickel(const Epub &epub, const BookRow &row, QString *xpointer, QString *error);

// Nickel's location ("path" or "path#fragment") for an XPointer: a kepub span
// ID for kepub files, an Adobe point for plain EPUB files, the chapter path
// alone when the position inside the chapter cannot be mapped.
bool nickelFromXPointer(const Epub &epub, const BookRow &row, const QString &xpointer, QString *location,
                        QString *error);

// A rough fraction (0..1) read, from the XPointer's chapter and the character
// position inside it, for when Nickel has no percentage for the position.
double fractionOfXPointer(const Epub &epub, const QString &xpointer);

}

#endif
