#!/bin/sh
set -eu

target="${1:-simulator}"

case "$target" in
  simulator)
    exec make simulator-smoke
    ;;
  hd-wf2|hd-wf2-raw-hub75)
    if [ "$target" = "hd-wf2" ]; then
      pio_environment="hd_wf2_build_only"
    else
      pio_environment="hd_wf2_raw_hub75_diagnostic"
    fi
    if command -v pio >/dev/null 2>&1; then
      pio_executable="pio"
    elif [ -x "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio" ]; then
      pio_executable="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio"
    else
      pio_executable=""
    fi
    if [ -n "$pio_executable" ]; then
      version="$("$pio_executable" --version | awk '{print $NF}')"
      if [ "$version" != "6.1.19" ]; then
        echo "PlatformIO 6.1.19 is required; found $version" >&2
        exit 2
      fi
      exec "$pio_executable" run --project-dir firmware/amg-flightwall --environment "$pio_environment"
    fi
    if python3 -c 'import platformio' >/dev/null 2>&1; then
      version="$(python3 -m platformio --version | awk '{print $NF}')"
      if [ "$version" != "6.1.19" ]; then
        echo "PlatformIO 6.1.19 is required; found $version" >&2
        exit 2
      fi
      exec python3 -m platformio run --project-dir firmware/amg-flightwall --environment "$pio_environment"
    fi
    echo "PlatformIO is required for the build-only HD-WF2 targets" >&2
    exit 2
    ;;
  *)
    echo "unsupported build target: $target" >&2
    echo "supported targets: simulator, hd-wf2, hd-wf2-raw-hub75" >&2
    exit 2
    ;;
esac
