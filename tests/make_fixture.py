#!/usr/bin/env python3
"""Writes a small EPUB for the unit tests, plus its KOReader partial MD5."""
import hashlib
import sys
import zipfile

CHAPTER = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title></head>
<body>
  <div class="c">
    <h1 id="h"><span class="koboSpan" id="kobo.1.1">{title}&nbsp;here</span></h1>
    <p><span class="koboSpan" id="kobo.2.1">First sentence. </span><span class="koboSpan" id="kobo.2.2">Second sentence.</span></p>
    <p><span class="koboSpan" id="kobo.3.1">Third &mdash; paragraph {filler}</span></p>
    <!-- a comment -->
    <p>Plain <i>text</i> tail</p>
  </div>
</body></html>
"""

OPF = """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Fixture Book</dc:title>
    <dc:creator>Test Author</dc:creator>
    <dc:identifier id="id">fixture</dc:identifier>
  </metadata>
  <manifest>
    <item id="c1" href="Text/ch1.xhtml" media-type="application/xhtml+xml"/>
    <item id="c2" href="Text/ch%202.xhtml" media-type="application/xhtml+xml"/>
    <item id="c3" href="../extra/ch3.xhtml" media-type="application/xhtml+xml"/>
    <item id="css" href="style.css" media-type="text/css"/>
  </manifest>
  <spine>
    <itemref idref="c1"/>
    <itemref idref="c2"/>
    <itemref idref="c3"/>
  </spine>
</package>
"""

CONTAINER = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""


def partial_md5(path):
    md5 = hashlib.md5()
    with open(path, "rb") as f:
        size = f.seek(0, 2)
        for i in range(-1, 11):
            offset = 0 if i < 0 else 1024 << (2 * i)
            if offset >= size:
                break
            f.seek(offset)
            sample = f.read(1024)
            if not sample:
                break
            md5.update(sample)
    return md5.hexdigest()


def main(path):
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml", CONTAINER, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/content.opf", OPF, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/style.css", "p { margin: 0 }", compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/Text/ch1.xhtml", CHAPTER.format(title="One", filler="x" * 100), compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("OEBPS/Text/ch 2.xhtml", CHAPTER.format(title="Two", filler="y" * 3000), compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("extra/ch3.xhtml", CHAPTER.format(title="Three", filler="z" * 50000), compress_type=zipfile.ZIP_DEFLATED)
    with open(path + ".md5", "w") as f:
        f.write(partial_md5(path) + "\n" + hashlib.md5(path.rsplit("/", 1)[-1].encode()).hexdigest() + "\n")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "tests/fixture.epub")
