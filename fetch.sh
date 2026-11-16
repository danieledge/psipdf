#!/bin/bash
# Fetches MuPDF 1.3's source (with the FreeType and libjpeg it ships) from
# the official release, checks its SHA-256, and unpacks it into third/mupdf
# (git-ignored: MuPDF is never committed here). Nothing in it is run: the
# Makefile compiles the files it names.
# Copyright (C) 2026 Dan Edge. AGPL-3.0-or-later: see LICENSE.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
URL=https://mupdf.com/downloads/archive/mupdf-1.3-source.tar.gz
SUM=aba8b31bee9cc0a16abedab5e31c81c65996cba5591e62a50a79bea2a63d4478
D=$HERE/third
mkdir -p "$D/dl"
[ -f "$D/dl/mupdf-1.3-source.tar.gz" ] || curl -sSL -o "$D/dl/mupdf-1.3-source.tar.gz" "$URL"
echo "$SUM  $D/dl/mupdf-1.3-source.tar.gz" | sha256sum -c -
rm -rf "$D/mupdf"; mkdir -p "$D/x"
tar xzf "$D/dl/mupdf-1.3-source.tar.gz" -C "$D/x"
mv "$D/x/mupdf-1.3-source" "$D/mupdf"; rmdir "$D/x"
echo "MuPDF 1.3 in $D/mupdf"
