#!/usr/bin/env bash
# ลิงก์ ./snake เข้าโฟลเดอร์ที่อยู่ใน PATH อยู่แล้ว เพื่อพิมพ์แค่ `snake` ได้ทุกที่ (ไม่แตะ ~/.zshrc)
#   ./install.sh             # ติดตั้ง
#   ./install.sh uninstall   # ถอนการติดตั้ง
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/snake"

on_path() {
    case ":$PATH:" in *":$1:"*) return 0 ;; esac
    return 1
}

if [ "${1:-}" = "uninstall" ]; then
    IFS=: read -ra DIRS <<< "$PATH"
    for d in "${DIRS[@]}"; do
        if [ -L "$d/snake" ] && [ "$(readlink "$d/snake")" = "$SRC" ]; then
            rm "$d/snake" && echo "ลบ $d/snake แล้ว"
        fi
    done
    exit 0
fi

# เลือกโฟลเดอร์แรกที่อยู่ใน PATH และเขียนได้
DEST=""
for d in "$HOME/.local/bin" /opt/homebrew/bin /usr/local/bin; do
    if on_path "$d" && [ -d "$d" ] && [ -w "$d" ]; then
        DEST="$d"
        break
    fi
done

if [ -z "$DEST" ]; then
    echo "ไม่เจอโฟลเดอร์ใน PATH ที่เขียนได้ ลองรันด้วย: sudo ln -s \"$SRC\" /usr/local/bin/snake" >&2
    exit 1
fi

TARGET="$DEST/snake"
if [ -e "$TARGET" ] || [ -L "$TARGET" ]; then
    if [ -L "$TARGET" ] && [ "$(readlink "$TARGET")" = "$SRC" ]; then
        echo "ติดตั้งไว้แล้วที่ $TARGET"
        exit 0
    fi
    echo "มีไฟล์ $TARGET อยู่แล้ว (ไม่ใช่ของโปรเจกต์นี้) — ไม่ทับให้" >&2
    exit 1
fi

ln -s "$SRC" "$TARGET"
echo "ติดตั้งแล้ว: $TARGET -> $SRC"
echo "พิมพ์ \`snake\` เพื่อเล่นได้เลย (shell ที่เปิดอยู่อาจต้องรัน \`hash -r\` ก่อน)"
