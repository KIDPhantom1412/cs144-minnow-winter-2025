#!/usr/bin/env bash
# Read a PDF via the WSL-side uv + PyMuPDF (see the read-pdf skill's SKILL.md).
# Dependencies are managed by uv from the PEP 723 inline metadata in read-pdf.py.
#
# Usage:
#   read-pdf.sh text   <file.pdf> [start] [end]
#   read-pdf.sh render <file.pdf> <start> <end> <outdir> [dpi]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mode=$1
pdf=$2
shift 2

case $mode in
  text)
    uv run "$SCRIPT_DIR/read-pdf.py" text "$pdf" "$@"
    ;;
  render)
    start=$1
    end=$2
    outdir=$3
    dpi=${4:-150}
    mkdir -p "$outdir"
    rm -f "$outdir"/*.png
    uv run "$SCRIPT_DIR/read-pdf.py" render "$pdf" "$start" "$end" "$outdir" "$dpi"
    ls "$outdir"/*.png
    ;;
  *)
    echo "unknown mode: $mode (expected 'text' or 'render')" >&2
    exit 2
    ;;
esac
