#!/usr/bin/env python3
"""Small dependency-free Markdown-to-PDF renderer for Korean study notes.

It intentionally implements only the Markdown subset used by the Pintos guide.
The PDF uses a predefined Korean CID font mapping so the script does not need
ReportLab, pandoc, LaTeX, or local CJK fonts.
"""

from __future__ import annotations

import re
import sys
import unicodedata
from pathlib import Path


PAGE_W = 595.28
PAGE_H = 841.89
LEFT = 50.0
RIGHT = 50.0
TOP = 58.0
BOTTOM = 58.0
CONTENT_W = PAGE_W - LEFT - RIGHT


def n(value: float) -> str:
    text = f"{value:.2f}"
    return text.rstrip("0").rstrip(".")


def hex_text(text: str) -> str:
    return "<" + text.encode("utf-16-be").hex().upper() + ">"


def display_units(text: str) -> float:
    total = 0.0
    for ch in text:
        if ch == "\t":
            total += 4.0
        elif ch == " ":
            total += 0.85
        elif unicodedata.east_asian_width(ch) in ("W", "F", "A"):
            total += 2.0
        else:
            total += 1.0
    return total


def wrap_units(text: str, max_units: float) -> list[str]:
    text = text.strip()
    if not text:
        return [""]

    lines: list[str] = []
    start = 0
    length = len(text)

    while start < length:
        units = 0.0
        last_space = -1
        end = start
        while end < length:
            ch = text[end]
            units += display_units(ch)
            if ch.isspace():
                last_space = end
            if units > max_units:
                break
            end += 1

        if end >= length:
            lines.append(text[start:].strip())
            break

        if last_space >= start:
            lines.append(text[start:last_space].strip())
            start = last_space + 1
        else:
            lines.append(text[start:end].strip())
            start = end

        while start < length and text[start].isspace():
            start += 1

    return lines


def wrap_code(text: str, max_chars: int) -> list[str]:
    if text == "":
        return [""]
    expanded = text.expandtabs(4)
    return [expanded[i : i + max_chars] for i in range(0, len(expanded), max_chars)]


def clean_inline(text: str) -> str:
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    text = text.replace("**", "")
    text = text.replace("__", "")
    text = text.replace("`", "")
    text = text.replace("<br>", " ")
    return text


class Renderer:
    def __init__(self, title: str) -> None:
        self.title = title
        self.pages: list[str] = []
        self.cmds: list[str] = []
        self.page_no = 0
        self.y = PAGE_H - TOP
        self.new_page()

    def new_page(self) -> None:
        if self.cmds:
            self.footer()
            self.pages.append("\n".join(self.cmds) + "\n")
        self.page_no += 1
        self.cmds = []
        self.y = PAGE_H - TOP
        self.text(self.title, LEFT, PAGE_H - 31, 8.0, color=(0.35, 0.35, 0.35))
        self.line(LEFT, PAGE_H - 42, PAGE_W - RIGHT, PAGE_H - 42, (0.82, 0.82, 0.82))

    def footer(self) -> None:
        self.line(LEFT, 42, PAGE_W - RIGHT, 42, (0.82, 0.82, 0.82))
        self.text(f"{self.page_no}", PAGE_W / 2 - 5, 25, 8.5, color=(0.35, 0.35, 0.35))

    def finish(self) -> None:
        if self.cmds:
            self.footer()
            self.pages.append("\n".join(self.cmds) + "\n")
            self.cmds = []

    def ensure(self, needed: float) -> None:
        if self.y - needed < BOTTOM:
            self.new_page()

    def text(
        self,
        text: str,
        x: float,
        y: float,
        size: float,
        font: str = "F1",
        color: tuple[float, float, float] = (0, 0, 0),
    ) -> None:
        r, g, b = color
        self.cmds.append(f"{n(r)} {n(g)} {n(b)} rg")
        if font == "F2":
            safe = text.encode("latin-1", "replace").decode("latin-1")
            escaped = safe.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
            payload = f"({escaped})"
        else:
            payload = hex_text(text)
        self.cmds.append(f"BT /{font} {n(size)} Tf {n(x)} {n(y)} Td {payload} Tj ET")

    def rect(self, x: float, y: float, w: float, h: float, color: tuple[float, float, float]) -> None:
        r, g, b = color
        self.cmds.append(f"{n(r)} {n(g)} {n(b)} rg {n(x)} {n(y)} {n(w)} {n(h)} re f")

    def line(
        self,
        x1: float,
        y1: float,
        x2: float,
        y2: float,
        color: tuple[float, float, float],
    ) -> None:
        r, g, b = color
        self.cmds.append(f"{n(r)} {n(g)} {n(b)} RG {n(x1)} {n(y1)} m {n(x2)} {n(y2)} l S")

    def space(self, amount: float) -> None:
        self.y -= amount

    def paragraph(
        self,
        text: str,
        size: float = 10.5,
        leading: float = 15.0,
        x: float = LEFT,
        max_w: float = CONTENT_W,
        color: tuple[float, float, float] = (0.07, 0.07, 0.07),
    ) -> None:
        max_units = max_w / (size * 0.53)
        for line in wrap_units(clean_inline(text), max_units):
            self.ensure(leading + 2)
            self.text(line, x, self.y, size, color=color)
            self.y -= leading

    def heading(self, text: str, level: int) -> None:
        text = clean_inline(text)
        if level == 1:
            self.ensure(62)
            self.y -= 10
            self.paragraph(text, size=20.0, leading=25.0, color=(0.05, 0.18, 0.32))
            self.line(LEFT, self.y + 5, PAGE_W - RIGHT, self.y + 5, (0.55, 0.68, 0.78))
            self.y -= 8
        elif level == 2:
            self.ensure(42)
            self.y -= 8
            self.paragraph(text, size=15.0, leading=20.0, color=(0.08, 0.25, 0.40))
            self.y -= 3
        else:
            self.ensure(34)
            self.y -= 5
            self.paragraph(text, size=12.3, leading=17.0, color=(0.12, 0.28, 0.38))

    def bullet(self, text: str, marker: str = "-") -> None:
        size = 10.0
        leading = 14.4
        x_marker = LEFT + 8
        x_text = LEFT + 25
        max_units = (CONTENT_W - 25) / (size * 0.53)
        lines = wrap_units(clean_inline(text), max_units)
        self.ensure(leading * len(lines) + 2)
        self.text(marker, x_marker, self.y, size, color=(0.1, 0.1, 0.1))
        for i, line in enumerate(lines):
            self.text(line, x_text, self.y - i * leading, size, color=(0.07, 0.07, 0.07))
        self.y -= leading * len(lines)

    def quote(self, text: str) -> None:
        self.ensure(30)
        self.rect(LEFT, self.y - 18, CONTENT_W, 23, (0.94, 0.96, 0.97))
        self.paragraph(clean_inline(text), size=9.8, leading=13.5, x=LEFT + 10, max_w=CONTENT_W - 20)
        self.y -= 2

    def code(self, lines: list[str]) -> None:
        size = 8.0
        leading = 10.5
        max_chars = int((CONTENT_W - 18) / (size * 0.60))
        self.y -= 2
        for raw in lines:
            for line in wrap_code(raw.rstrip("\n"), max_chars):
                self.ensure(leading + 8)
                self.rect(LEFT, self.y - 2, CONTENT_W, leading + 2, (0.95, 0.95, 0.93))
                self.text(line, LEFT + 8, self.y, size, font="F2", color=(0.08, 0.08, 0.08))
                self.y -= leading
        self.y -= 5


def render_markdown(md: str) -> Renderer:
    title = "Pintos 학습 가이드"
    for line in md.splitlines():
        if line.startswith("# "):
            title = clean_inline(line[2:].strip())
            break

    renderer = Renderer(title)
    paragraph: list[str] = []
    code_lines: list[str] = []
    in_code = False

    def flush_paragraph() -> None:
        if paragraph:
            renderer.paragraph(" ".join(paragraph))
            renderer.space(4)
            paragraph.clear()

    for raw in md.splitlines():
        line = raw.rstrip()
        stripped = line.strip()

        if stripped.startswith("```"):
            if in_code:
                renderer.code(code_lines)
                code_lines = []
                in_code = False
            else:
                flush_paragraph()
                in_code = True
            continue

        if in_code:
            code_lines.append(line)
            continue

        if stripped == "<!-- pagebreak -->":
            flush_paragraph()
            renderer.new_page()
            continue

        if not stripped:
            flush_paragraph()
            renderer.space(4)
            continue

        if stripped == "---":
            flush_paragraph()
            renderer.ensure(16)
            renderer.line(LEFT, renderer.y, PAGE_W - RIGHT, renderer.y, (0.72, 0.72, 0.72))
            renderer.space(14)
            continue

        match = re.match(r"^(#{1,3})\s+(.*)$", stripped)
        if match:
            flush_paragraph()
            renderer.heading(match.group(2), len(match.group(1)))
            continue

        match = re.match(r"^[-*]\s+(.*)$", stripped)
        if match:
            flush_paragraph()
            renderer.bullet(match.group(1), "-")
            continue

        match = re.match(r"^(\d+)\.\s+(.*)$", stripped)
        if match:
            flush_paragraph()
            renderer.bullet(match.group(2), match.group(1) + ".")
            continue

        if stripped.startswith(">"):
            flush_paragraph()
            renderer.quote(stripped.lstrip("> "))
            continue

        paragraph.append(stripped)

    flush_paragraph()
    renderer.finish()
    return renderer


def pdf_object(body: bytes) -> bytes:
    return body


def build_pdf(pages: list[str]) -> bytes:
    objects: list[bytes] = []

    def add(body: str | bytes) -> int:
        if isinstance(body, str):
            body_b = body.encode("latin-1")
        else:
            body_b = body
        objects.append(pdf_object(body_b))
        return len(objects)

    catalog_id = add("PLACEHOLDER")
    pages_id = add("PLACEHOLDER")
    korean_desc_id = add(
        "<< /Type /Font /Subtype /CIDFontType0 /BaseFont /HYSMyeongJo-Medium "
        "/CIDSystemInfo << /Registry (Adobe) /Ordering (Korea1) /Supplement 2 >> >>"
    )
    korean_id = add(
        f"<< /Type /Font /Subtype /Type0 /BaseFont /HYSMyeongJo-Medium "
        f"/Encoding /UniKS-UCS2-H /DescendantFonts [{korean_desc_id} 0 R] >>"
    )
    courier_id = add("<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")

    page_ids: list[int] = []
    for page in pages:
        content = page.encode("latin-1")
        content_id = add(b"<< /Length " + str(len(content)).encode("ascii") + b" >>\nstream\n" + content + b"endstream")
        page_id = add(
            f"<< /Type /Page /Parent {pages_id} 0 R /MediaBox [0 0 {n(PAGE_W)} {n(PAGE_H)}] "
            f"/Resources << /Font << /F1 {korean_id} 0 R /F2 {courier_id} 0 R >> >> "
            f"/Contents {content_id} 0 R >>"
        )
        page_ids.append(page_id)

    kids = " ".join(f"{page_id} 0 R" for page_id in page_ids)
    objects[catalog_id - 1] = f"<< /Type /Catalog /Pages {pages_id} 0 R >>".encode("latin-1")
    objects[pages_id - 1] = f"<< /Type /Pages /Kids [{kids}] /Count {len(page_ids)} >>".encode("latin-1")

    out = bytearray()
    out.extend(b"%PDF-1.4\n%\xE2\xE3\xCF\xD3\n")
    offsets = [0]
    for idx, body in enumerate(objects, start=1):
        offsets.append(len(out))
        out.extend(f"{idx} 0 obj\n".encode("ascii"))
        out.extend(body)
        out.extend(b"\nendobj\n")
    xref_pos = len(out)
    out.extend(f"xref\n0 {len(objects) + 1}\n".encode("ascii"))
    out.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        out.extend(f"{offset:010d} 00000 n \n".encode("ascii"))
    out.extend(
        f"trailer\n<< /Size {len(objects) + 1} /Root {catalog_id} 0 R >>\n"
        f"startxref\n{xref_pos}\n%%EOF\n".encode("ascii")
    )
    return bytes(out)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: md_to_simple_pdf.py INPUT.md OUTPUT.pdf", file=sys.stderr)
        return 2
    source = Path(argv[1])
    target = Path(argv[2])
    renderer = render_markdown(source.read_text(encoding="utf-8"))
    target.write_bytes(build_pdf(renderer.pages))
    print(f"wrote {target} ({len(renderer.pages)} pages)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
