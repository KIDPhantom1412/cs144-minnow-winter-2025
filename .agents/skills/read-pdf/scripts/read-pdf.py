# /// script
# requires-python = ">=3.11"
# dependencies = ["pymupdf"]
# ///
"""Extract text from or render pages of a PDF.

Run from WSL via uv (usually through the read-pdf.sh wrapper in the same
directory):

    uv run read-pdf.py text handouts/check0.pdf [start] [end]
    uv run read-pdf.py render handouts/check0.pdf start end outdir [dpi]
"""

import argparse
import sys
from pathlib import Path

import pymupdf


def page_range(doc, start, end):
    first = (start or 1) - 1
    last = min(end or doc.page_count, doc.page_count)
    return range(max(first, 0), last)


def cmd_text(args):
    with pymupdf.open(args.pdf) as doc:
        for pno in page_range(doc, args.start, args.end):
            print(f"===== page {pno + 1} =====")
            print(doc[pno].get_text())


def cmd_render(args):
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    with pymupdf.open(args.pdf) as doc:
        for pno in page_range(doc, args.start, args.end):
            out = outdir / f"page-{pno + 1}.png"
            doc[pno].get_pixmap(dpi=args.dpi).save(out)
            print(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_text = sub.add_parser("text", help="print page text to stdout")
    p_text.add_argument("pdf")
    p_text.add_argument("start", nargs="?", type=int, default=None, help="first page, 1-based")
    p_text.add_argument("end", nargs="?", type=int, default=None, help="last page, inclusive")
    p_text.set_defaults(func=cmd_text)

    p_render = sub.add_parser("render", help="render pages to PNG files")
    p_render.add_argument("pdf")
    p_render.add_argument("start", type=int, help="first page, 1-based")
    p_render.add_argument("end", type=int, help="last page, inclusive")
    p_render.add_argument("outdir")
    p_render.add_argument("dpi", nargs="?", type=int, default=150)
    p_render.set_defaults(func=cmd_render)

    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    args.func(args)


if __name__ == "__main__":
    main()
