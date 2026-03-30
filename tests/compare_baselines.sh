#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HISTORY_FILE="${ROOT_DIR}/metrics/baselines/history.tsv"

WARN_CONTRACT="${TKVM_BASELINE_WARN_CONTRACT_SEC:-}"
WARN_CAPI="${TKVM_BASELINE_WARN_CAPI_SEC:-}"
WARN_FULL="${TKVM_BASELINE_WARN_FULL_SEC:-}"
FAIL_CONTRACT="${TKVM_BASELINE_FAIL_CONTRACT_SEC:-}"
FAIL_CAPI="${TKVM_BASELINE_FAIL_CAPI_SEC:-}"
FAIL_FULL="${TKVM_BASELINE_FAIL_FULL_SEC:-}"

while [[ $# -gt 0 ]]; do
	case "$1" in
	--warn-contract)
		WARN_CONTRACT="${2:-}"
		shift 2
		;;
	--warn-capi)
		WARN_CAPI="${2:-}"
		shift 2
		;;
	--warn-full)
		WARN_FULL="${2:-}"
		shift 2
		;;
	--fail-contract)
		FAIL_CONTRACT="${2:-}"
		shift 2
		;;
	--fail-capi)
		FAIL_CAPI="${2:-}"
		shift 2
		;;
	--fail-full)
		FAIL_FULL="${2:-}"
		shift 2
		;;
	--help|-h)
		echo "Usage: $0 [--warn-contract <sec>] [--warn-capi <sec>] [--warn-full <sec>] [--fail-contract <sec>] [--fail-capi <sec>] [--fail-full <sec>]"
		exit 0
		;;
	*)
		echo "Unknown argument: $1"
		exit 2
		;;
	esac
done

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

parse_history_row() {
	local row="$1"
	local -a f=()
	IFS=$'\t' read -r -a f <<< "${row}"
	if [[ ${#f[@]} -ge 11 ]]; then
		echo "${f[0]}|${f[1]}|${f[2]}|${f[3]}|${f[4]}|${f[5]}|${f[6]}|${f[7]}|${f[8]}|${f[9]}|${f[10]}"
		return
	fi
	if [[ ${#f[@]} -ge 9 ]]; then
		echo "${f[0]}|${f[1]}|${f[2]}|na|nan|${f[3]}|${f[4]}|${f[5]}|${f[6]}|${f[7]}|${f[8]}"
		return
	fi
	echo "invalid||||||||||"
}

IFS='|' read -r PREV_TS PREV_PHASE PREV_COMMIT PREV_CONTRACT_STATUS PREV_CONTRACT_SEC PREV_CAPI_STATUS PREV_CAPI_SEC PREV_FULL_STATUS PREV_FULL_SEC PREV_SIMPLE PREV_TINY <<< "$(parse_history_row "${PREV}")"
IFS='|' read -r CURR_TS CURR_PHASE CURR_COMMIT CURR_CONTRACT_STATUS CURR_CONTRACT_SEC CURR_CAPI_STATUS CURR_CAPI_SEC CURR_FULL_STATUS CURR_FULL_SEC CURR_SIMPLE CURR_TINY <<< "$(parse_history_row "${CURR}")"

awk_delta() {
	awk -v a="$1" -v b="$2" 'BEGIN { if (a=="nan" || b=="nan") print "nan"; else printf "%.3f", (b-a) }'
}

is_gt() {
	awk -v a="$1" -v b="$2" 'BEGIN { exit !(a > b) }'
}

evaluate_threshold() {
	local lane="$1"
	local delta="$2"
	local warn="$3"
	local fail="$4"
	local out_msg
	out_msg=""

	if [[ "${delta}" == "nan" ]]; then
		echo ""
		return
	fi

	if [[ -n "${fail}" ]] && is_gt "${delta}" "${fail}"; then
		echo "FAIL ${lane} delta ${delta}s exceeded fail threshold ${fail}s"
		return
	fi
	if [[ -n "${warn}" ]] && is_gt "${delta}" "${warn}"; then
		echo "WARN ${lane} delta ${delta}s exceeded warn threshold ${warn}s"
		return
	fi
	echo ""
}

DELTA_CONTRACT="$(awk_delta "${PREV_CONTRACT_SEC}" "${CURR_CONTRACT_SEC}")"
DELTA_CAPI="$(awk_delta "${PREV_CAPI_SEC}" "${CURR_CAPI_SEC}")"
DELTA_FULL="$(awk_delta "${PREV_FULL_SEC}" "${CURR_FULL_SEC}")"
DELTA_SIMPLE="$((CURR_SIMPLE - PREV_SIMPLE))"
DELTA_TINY="$((CURR_TINY - PREV_TINY))"

echo "Comparing baseline checkpoints"
echo "Previous: ${PREV_PHASE} (${PREV_COMMIT}) @ ${PREV_TS}"
echo "Current : ${CURR_PHASE} (${CURR_COMMIT}) @ ${CURR_TS}"
echo
echo "contract_label_seconds: ${PREV_CONTRACT_SEC} -> ${CURR_CONTRACT_SEC} (delta ${DELTA_CONTRACT})"
echo "c_api_label_seconds: ${PREV_CAPI_SEC} -> ${CURR_CAPI_SEC} (delta ${DELTA_CAPI})"
echo "full_label_seconds : ${PREV_FULL_SEC} -> ${CURR_FULL_SEC} (delta ${DELTA_FULL})"
echo "simplekvm_c_bytes  : ${PREV_SIMPLE} -> ${CURR_SIMPLE} (delta ${DELTA_SIMPLE})"
echo "tinytest_c_bytes   : ${PREV_TINY} -> ${CURR_TINY} (delta ${DELTA_TINY})"
echo "contract_status    : ${PREV_CONTRACT_STATUS} -> ${CURR_CONTRACT_STATUS}"
echo "c_api_status       : ${PREV_CAPI_STATUS} -> ${CURR_CAPI_STATUS}"
echo "full_status        : ${PREV_FULL_STATUS} -> ${CURR_FULL_STATUS}"

WARN_COUNT=0
FAIL_COUNT=0

for msg in \
	"$(evaluate_threshold contract "${DELTA_CONTRACT}" "${WARN_CONTRACT}" "${FAIL_CONTRACT}")" \
	"$(evaluate_threshold c_api "${DELTA_CAPI}" "${WARN_CAPI}" "${FAIL_CAPI}")" \
	"$(evaluate_threshold full "${DELTA_FULL}" "${WARN_FULL}" "${FAIL_FULL}")"; do
	if [[ -z "${msg}" ]]; then
		continue
	fi
	echo "${msg}"
	if [[ "${msg}" == WARN* ]]; then
		WARN_COUNT=$((WARN_COUNT + 1))
	fi
	if [[ "${msg}" == FAIL* ]]; then
		FAIL_COUNT=$((FAIL_COUNT + 1))
	fi
done

if [[ ${WARN_COUNT} -gt 0 || ${FAIL_COUNT} -gt 0 ]]; then
	echo
	echo "Threshold summary: ${WARN_COUNT} warnings, ${FAIL_COUNT} failures"
fi

if [[ ${FAIL_COUNT} -gt 0 ]]; then
	exit 1
fi
