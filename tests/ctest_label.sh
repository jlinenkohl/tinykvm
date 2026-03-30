#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <label> [--metrics]"
	exit 2
fi

LABEL="$1"
METRICS_MODE=0
if [[ "${2:-}" == "--metrics" ]]; then
	METRICS_MODE=1
fi

LOG_FILE="$(mktemp)"
set +e
ctest --test-dir build -L "${LABEL}" --output-on-failure > "${LOG_FILE}" 2>&1
RC=$?
set -e

STATUS="pass"
if [[ ${RC} -ne 0 ]]; then
	STATUS="fail"
fi
SECONDS_REAL="$(awk '/Total Test time \(real\) =/{print $(NF-1)}' "${LOG_FILE}" | tail -n1)"
if [[ -z "${SECONDS_REAL}" ]]; then
	SECONDS_REAL="nan"
fi

if [[ ${METRICS_MODE} -eq 1 ]]; then
	cat "${LOG_FILE}" >&2
	echo "${STATUS},${SECONDS_REAL},${RC}"
	rm -f "${LOG_FILE}"
	exit 0
fi

cat "${LOG_FILE}"
rm -f "${LOG_FILE}"
exit ${RC}
