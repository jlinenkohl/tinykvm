#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <phase-label>"
	echo "Example: $0 phase11a"
	exit 2
fi

PHASE="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

OUT_DIR="metrics/baselines"
mkdir -p "${OUT_DIR}"

COMMIT="$(git rev-parse --short HEAD)"
TS_UTC="$(date -u +%Y%m%dT%H%M%SZ)"
SNAPSHOT_FILE="${OUT_DIR}/${PHASE}_${COMMIT}_${TS_UTC}.baseline"
HISTORY_FILE="${OUT_DIR}/history.tsv"

ensure_history_schema() {
	if [[ ! -f "${HISTORY_FILE}" ]]; then
		echo -e "timestamp_utc\tphase\tcommit\tcontract_status\tcontract_seconds\tc_api_status\tc_api_seconds\tfull_status\tfull_seconds\tsimplekvm_c_bytes\ttinytest_c_bytes" > "${HISTORY_FILE}"
		return
	fi

	local header
	header="$(head -n1 "${HISTORY_FILE}")"
	if [[ "${header}" == *$'\tcontract_status\tcontract_seconds\t'* ]]; then
		return
	fi

	local tmp
	tmp="$(mktemp)"
	echo -e "timestamp_utc\tphase\tcommit\tcontract_status\tcontract_seconds\tc_api_status\tc_api_seconds\tfull_status\tfull_seconds\tsimplekvm_c_bytes\ttinytest_c_bytes" > "${tmp}"
	tail -n +2 "${HISTORY_FILE}" | while IFS=$'\t' read -r ts phase commit c_api_status c_api_seconds full_status full_seconds simple_bytes tiny_bytes; do
		[[ -z "${ts}" ]] && continue
		echo -e "${ts}\t${phase}\t${commit}\tna\tnan\t${c_api_status}\t${c_api_seconds}\t${full_status}\t${full_seconds}\t${simple_bytes}\t${tiny_bytes}" >> "${tmp}"
	done
	mv "${tmp}" "${HISTORY_FILE}"
}

cmake -S . -B build
cmake --build build -j --target capi_smoke capi_contract_smoke simplekvm_c tinytest_c

IFS=',' read -r CONTRACT_STATUS CONTRACT_SECONDS CONTRACT_RC < <("${SCRIPT_DIR}/ctest_label.sh" contract --metrics)
IFS=',' read -r CAPI_STATUS CAPI_SECONDS CAPI_RC < <("${SCRIPT_DIR}/ctest_label.sh" c_api --metrics)
IFS=',' read -r FULL_STATUS FULL_SECONDS FULL_RC < <("${SCRIPT_DIR}/ctest_label.sh" full --metrics)

SIMPLEKVM_C_BYTES="$(stat -c%s build/simplekvm_c)"
TINYTEST_C_BYTES="$(stat -c%s build/tinytest_c)"

cat > "${SNAPSHOT_FILE}" <<EOF
# tinykvm baseline snapshot
phase=${PHASE}
commit=${COMMIT}
timestamp_utc=${TS_UTC}

contract_label_status=${CONTRACT_STATUS}
contract_label_seconds=${CONTRACT_SECONDS}
contract_label_exit_code=${CONTRACT_RC}

c_api_label_status=${CAPI_STATUS}
c_api_label_seconds=${CAPI_SECONDS}
c_api_label_exit_code=${CAPI_RC}

full_label_status=${FULL_STATUS}
full_label_seconds=${FULL_SECONDS}
full_label_exit_code=${FULL_RC}

simplekvm_c_bytes=${SIMPLEKVM_C_BYTES}
tinytest_c_bytes=${TINYTEST_C_BYTES}
EOF

ensure_history_schema

echo -e "${TS_UTC}\t${PHASE}\t${COMMIT}\t${CONTRACT_STATUS}\t${CONTRACT_SECONDS}\t${CAPI_STATUS}\t${CAPI_SECONDS}\t${FULL_STATUS}\t${FULL_SECONDS}\t${SIMPLEKVM_C_BYTES}\t${TINYTEST_C_BYTES}" >> "${HISTORY_FILE}"

echo "Wrote snapshot: ${SNAPSHOT_FILE}"
echo "Updated history: ${HISTORY_FILE}"

if [[ "${CONTRACT_STATUS}" != "pass" || "${CAPI_STATUS}" != "pass" || "${FULL_STATUS}" != "pass" ]]; then
	echo "One or more baseline lanes failed"
	exit 1
fi
