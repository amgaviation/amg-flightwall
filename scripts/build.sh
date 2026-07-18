#!/bin/sh
set -eu

target="${1:-simulator}"

case "$target" in
  simulator)
    exec make simulator-smoke
    ;;
  hd-wf2)
    if command -v pio >/dev/null 2>&1; then
      version="$(pio --version | awk '{print $NF}')"
      if [ "$version" != "6.1.19" ]; then
        echo "PlatformIO 6.1.19 is required; found $version" >&2
        exit 2
      fi
      exec pio run --project-dir firmware/amg-flightwall --environment hd_wf2_build_only
    fi
    if python3 -c 'import platformio' >/dev/null 2>&1; then
      version="$(python3 -m platformio --version | awk '{print $NF}')"
      if [ "$version" != "6.1.19" ]; then
        echo "PlatformIO 6.1.19 is required; found $version" >&2
        exit 2
      fi
      exec python3 -m platformio run --project-dir firmware/amg-flightwall --environment hd_wf2_build_only
    fi
    echo "PlatformIO is required for the build-only HD-WF2 target" >&2
    exit 2
    ;;
  *)
    echo "unsupported build target: $target" >&2
    echo "supported targets: simulator, hd-wf2" >&2
    exit 2
    ;;
esac
