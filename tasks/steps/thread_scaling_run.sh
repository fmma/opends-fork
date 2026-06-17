#!/bin/bash
# Run the thread-scaling read benchmark for one backend, sweeping thread count
# (and reactor count for aisio). Invoked once per backend by
# bench_thread_scaling.yaml after that backend's device state is set up
# (gds: kernel nvme + mount + nvidia_fs; aisio/aisio_legacy: HOMI/qublk).
#
# Backend- and dataset-agnostic: every value is an explicit flag, so the caller
# is the single source of truth and a missing one fails loudly. The only env it
# sets is the aisio library's own runtime interface (OPENDS_HOMI_DEV /
# OPENDS_HOMI_SOCKET / OPENDS_AISIO_REACTORS), not bench config.
#
# The harness sweeps each file once (no reread); a run ends when the dataset is
# exhausted, so --timeout is a watchdog against a wedged read, not the run length.
#
# flags:
#   --build-dir DIR    holds the bench_thread_scaling_<backend> binaries
#   --backend NAME     gds | aisio | aisio_legacy
#   --data DIR         dataset directory (read recursively)
#   --read-bytes N     bytes read per file
#   --read-offset N       per-file read offset (0 = from start)
#   --buffers N        distinct GPU buffers, divided across threads
#   --threads "LIST"   space-separated thread counts to sweep
#   --timeout SECS     per-run watchdog
#   --nvme-bdf BDF     controller BDF (aisio/aisio_legacy only)
#   --reactors "LIST"  space-separated reactor counts to sweep (aisio only)
set -euo pipefail

BUILD_DIR= BACKEND= DATA= READ_BYTES= READ_OFFSET= BUFFERS= THREADS= TIMEOUT= NVME_BDF= REACTORS=
while [ $# -gt 0 ]; do
	case "$1" in
	--build-dir)  BUILD_DIR=$2;  shift 2 ;;
	--backend)    BACKEND=$2;    shift 2 ;;
	--data)       DATA=$2;       shift 2 ;;
	--read-bytes) READ_BYTES=$2; shift 2 ;;
	--read-offset)   READ_OFFSET=$2;   shift 2 ;;
	--buffers)    BUFFERS=$2;    shift 2 ;;
	--threads)    THREADS=$2;    shift 2 ;;
	--timeout)    TIMEOUT=$2;    shift 2 ;;
	--nvme-bdf)   NVME_BDF=$2;   shift 2 ;;
	--reactors)   REACTORS=$2;   shift 2 ;;
	*) echo "unknown flag: $1" >&2; exit 2 ;;
	esac
done

: "${BUILD_DIR:?--build-dir}" "${BACKEND:?--backend}" "${DATA:?--data}"
: "${READ_BYTES:?--read-bytes}" "${READ_OFFSET:?--read-offset}" "${BUFFERS:?--buffers}"
: "${THREADS:?--threads}" "${TIMEOUT:?--timeout}"

BIN="$BUILD_DIR/bench_thread_scaling_$BACKEND"
[ -x "$BIN" ] || { echo "missing $BIN (build OpenDS with the $BACKEND backend)"; exit 1; }
[ -d "$DATA" ] || { echo "missing dataset dir: $DATA"; exit 1; }
echo "backend=$BACKEND data=$DATA read_bytes=$READ_BYTES timeout_secs=$TIMEOUT online_cpus=$(nproc)"

# Binary flags shared by every run; the sweep appends --threads per iteration.
ARGS=(--data "$DATA" --read-bytes "$READ_BYTES" --read-offset "$READ_OFFSET" \
      --buffers "$BUFFERS" --timeout "$TIMEOUT")

# Only the aisio library reads env; gds reads none. aisio_legacy isn't
# thread-safe, so the binary serializes reads (matching the old binding lock).
ENVV=()
case "$BACKEND" in
aisio)
	: "${NVME_BDF:?--nvme-bdf required for aisio}" "${REACTORS:?--reactors required for aisio}"
	ENVV=(OPENDS_HOMI_DEV="$NVME_BDF" OPENDS_HOMI_SOCKET=/run/homi/homi.sock) ;;
aisio_legacy)
	: "${NVME_BDF:?--nvme-bdf required for aisio_legacy}"
	ENVV=(OPENDS_HOMI_DEV="$NVME_BDF" OPENDS_HOMI_SOCKET=/run/homi/homi.sock)
	ARGS+=(--serialize) ;;
esac

run_one() {
	local line
	line=$(env "${ENVV[@]}" "$@" "$BIN" "${ARGS[@]}" --threads "$K" \
		| grep '^RESULT' || true)
	[ -n "$line" ] || line="RESULT backend=$BACKEND threads=$K GiB_s=0.000 reads=0 lat_us_p50=0 lat_us_p99=0 errors=FAIL"
	echo "$line"
}

if [ "$BACKEND" = aisio ]; then
	for R in $REACTORS; do
		for K in $THREADS; do
			run_one OPENDS_AISIO_REACTORS="$R"
		done
	done
else
	for K in $THREADS; do
		run_one
	done
fi
