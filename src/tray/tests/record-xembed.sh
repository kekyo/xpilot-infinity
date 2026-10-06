#!/bin/sh
set -eu
record_dir=$(mktemp -d)
recorder=
finish()
{
    if test -n "$recorder"; then kill -INT "$recorder" 2>/dev/null || true; wait "$recorder" || true; fi
    rm -f "$record_dir/progress"
    rmdir "$record_dir"
}
trap finish EXIT
mkfifo "$record_dir/progress"
ffmpeg -hide_banner -loglevel error -nostdin -f x11grab -video_size 640x480 \
    -framerate 60 -i "$DISPLAY" -c:v ffv1 -y test-xembed.mkv -progress "$record_dir/progress" &
recorder=$!
exec 3< "$record_dir/progress"
# Start the lifecycle test after the first captured frame, not an arbitrary delay.
while IFS= read -r progress <&3; do
    case "$progress" in frame=*) test "${progress#frame=}" -gt 0 && break ;; esac
done
G_DEBUG=fatal-warnings "$@"
kill -INT "$recorder"
wait "$recorder" || test "$?" = 255
recorder=
ffprobe -v error -select_streams v:0 -count_frames -show_entries stream=nb_read_frames \
    -of default=noprint_wrappers=1:nokey=1 test-xembed.mkv | awk '{if ($1 < 2) exit 1}'
