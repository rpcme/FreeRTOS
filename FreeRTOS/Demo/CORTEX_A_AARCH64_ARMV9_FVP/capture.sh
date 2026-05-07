#!/bin/bash
# capture.sh — Grab a frame from webcam, save as input.pgm for the FVP demo
#
# Usage: ./capture.sh video1          (single frame from /dev/video1)
#        ./capture.sh video1 loop     (continuous capture every 2s)
#
# Requires: ffmpeg

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/input.pgm"
DEVICE="/dev/${1:-video1}"
SIZE="${CAPTURE_SIZE:-256x256}"

capture_one() {
    if ! ffmpeg -f v4l2 -i "$DEVICE" -frames:v 1 \
        -vf "scale=${SIZE},format=gray" \
        -y "$OUT" 2>/dev/null; then
        echo "ERROR: capture failed. Check:"
        echo "  - Camera exists: ls $DEVICE"
        echo "  - ffmpeg installed: which ffmpeg"
        return 1
    fi
    echo "Captured → $OUT"
}

if [ "$2" = "loop" ]; then
    echo "Continuous capture from $DEVICE → $OUT"
    echo "Press Ctrl+C to stop"
    while true; do
        capture_one || true
        sleep 2
    done
else
    capture_one
fi
