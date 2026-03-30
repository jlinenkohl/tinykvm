#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HISTORY_FILE="${ROOT_DIR}/metrics/baselines/history.tsv"

if [[ ! -f "${HISTORY_FILE}" ]]; then
	echo "No baseline history found: ${HISTORY_FILE}"
	exit 1
fi

LINE_COUNT="$(wc -l < "${HISTORY_FILE}")"
if [[ "${LINE_COUNT}" -lt 3 ]]; then
	echo "Need at least two baseline entries to compare"
	tail -n +1 "${HISTORY_FILE}"
	exit 0
fi

PREV="$(tail -n 2 "${HISTORY_FILE}" | head -n 1)"
CURR="$(tail -n 1 "${HISTORY_FILE}")"

IFS=$'\t' read -r PREV_TS PREV_PHASE PREV_COMMIT PREV_CAPI_STATUS PREV_CAPI_SEC PREV_FULL_STATUS PREV_FULL_SEC PREV_SIMPLE PREV_TINY <<< "${PREV}"
IFS=$'\t' read -r CURR_TS CURR_PHASE CURR_COMMIT CURR_CAPI_STATUS CURR_CAPI_SEC CURR_FULL_STATUS CURR_FULL_SEC CURR_SIMPLE CURR_TINY <<< "${CURR}"

awk_delta() {
	awk -v a="$1" -v b="$2" 'BEGIN { if (a=="nan" || b=="nan") print "nan"; else printf "%.3f", (b-a) }'
}

DELTA_CAPI="$(awk_delta "${PREV_CAPI_SEC}" "${CURR_CAPI_SEC}")"
DELTA_FULL="$(awk_delta "${PREV_FULL_SEC}" "${CURR_FULL_SEC}")"
DELTA_SIMPLE="$((CURR_SIMPLE - PREV_SIMPLE))"
DELTA_TINY="$((CURR_TINY - PREV_TINY))"

echo "Comparing baseline checkpoints"
echo "Previous: ${PREV_PHASE} (${PREV_COMMIT}) @ ${PREV_TS}"
echo "Current : ${CURR_PHASE} (${CURR_COMMIT}) @ ${CURR_TS}"
echo
echo "c_api_label_seconds: ${PREV_CAPI_SEC} -> ${CURR_CAPI_SEC} (delta ${DELTA_CAPI})"
echo "full_label_seconds : ${PREV_FULL_SEC} -> ${CURR_FULL_SEC} (delta ${DELTA_FULL})"
echo "simplekvm_c_bytes  : ${PREV_SIMPLE} -> ${CURR_SIMPLE} (delta ${DELTA_SIMPLE})"
echo "tinytest_c_bytes   : ${PREV_TINY} -> ${CURR_TINY} (delta ${DELTA_TINY})"
echo "c_api_status       : ${PREV_CAPI_STATUS} -> ${CURR_CAPI_STATUS}"
echo "full_status        : ${PREV_FULL_STATUS} -> ${CURR_FULL_STATUS}"
