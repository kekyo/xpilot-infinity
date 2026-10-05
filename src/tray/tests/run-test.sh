#!/bin/sh
set -eu
case "$1" in
    *test-xembed) exec xvfb-run -a -s '-screen 0 640x480x24' sh "$(dirname "$0")/record-xembed.sh" "$@" ;;
    *) exec "$@" ;;
esac
