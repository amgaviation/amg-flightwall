#!/bin/sh
set -eu

repository_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$repository_root"

if [ "$#" -gt 1 ]; then
  echo "usage: $0 [serial-port]" >&2
  exit 2
fi

for required_command in python3 shasum awk; do
  if ! command -v "$required_command" >/dev/null 2>&1; then
    echo "hardware smoke preflight: missing command: $required_command" >&2
    exit 2
  fi
done

if command -v pio >/dev/null 2>&1; then
  pio_mode="binary"
  pio_executable="pio"
elif [ -x "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio" ]; then
  pio_mode="binary"
  pio_executable="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio"
elif python3 -c 'import platformio' >/dev/null 2>&1; then
  pio_mode="module"
else
  echo "hardware smoke preflight: PlatformIO is required" >&2
  exit 2
fi

run_pio() {
  if [ "$pio_mode" = "binary" ]; then
    "$pio_executable" "$@"
  else
    python3 -m platformio "$@"
  fi
}

pio_version="$(run_pio --version | awk '{print $NF}')"
if [ "$pio_version" != "6.1.19" ]; then
  echo "hardware smoke preflight: PlatformIO 6.1.19 is required; found $pio_version" >&2
  exit 2
fi

manifest="backups/manifests/flightwall-mini-hd-wf2-20260718.json"
backup="backups/local/flightwall-mini-hd-wf2-factory-20260718T190327Z.bin"

if [ ! -f "$manifest" ] || [ ! -f "$backup" ]; then
  echo "hardware smoke preflight: local recovery evidence is missing" >&2
  exit 1
fi

expected_hash="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["sha256"])' "$manifest")"
actual_hash="$(shasum -a 256 "$backup" | awk '{print $1}')"
if [ "$actual_hash" != "$expected_hash" ]; then
  echo "hardware smoke preflight: factory backup hash mismatch" >&2
  exit 1
fi

if [ "$#" -gt 0 ]; then
  serial_port="$1"
  if [ ! -c "$serial_port" ]; then
    echo "hardware smoke preflight: serial character device not found: $serial_port" >&2
    exit 1
  fi
  echo "serial candidate present: $serial_port (not opened)"
fi

./scripts/check.sh
run_pio run --project-dir firmware/amg-flightwall --environment hd_wf2_build_only

firmware="firmware/amg-flightwall/.pio/build/hd_wf2_build_only/firmware.bin"
if [ ! -s "$firmware" ]; then
  echo "hardware smoke preflight: embedded artifact is missing" >&2
  exit 1
fi
firmware_hash="$(shasum -a 256 "$firmware" | awk '{print $1}')"

for blocked_target in upload uploadfs program erase; do
  set +e
  blocked_output="$(run_pio run --project-dir firmware/amg-flightwall \
    --environment hd_wf2_build_only --target "$blocked_target" \
    --upload-port /dev/null 2>&1)"
  blocked_status=$?
  set -e
  if [ "$blocked_status" -eq 0 ] ||
     ! printf '%s\n' "$blocked_output" | grep -Fq "AMG safety gate: device writes are disabled"; then
    echo "hardware smoke preflight: safety gate check failed for $blocked_target" >&2
    exit 1
  fi
done

echo "hardware smoke preflight: READY (compile-only; device writes remain locked)"
echo "factory backup sha256: $actual_hash"
echo "compiled firmware sha256: $firmware_hash"
