#!/bin/sh
set -eu

target="${1:-simulator}"

case "$target" in
  simulator)
    exec make simulator-smoke
    ;;
  *)
    echo "unsupported build target: $target" >&2
    echo "supported targets: simulator" >&2
    exit 2
    ;;
esac
