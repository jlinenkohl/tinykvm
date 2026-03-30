#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <phase-label> [threshold overrides]"
	echo "Example: $0 phase12e --warn-full 0.30 --fail-full 1.00"
	exit 2
fi

PHASE="$1"
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

# Default gating policy: warn on modest regressions, fail only on larger deltas.
WARN_CONTRACT="${TKVM_BASELINE_WARN_CONTRACT_SEC:-0.01}"
WARN_CAPI="${TKVM_BASELINE_WARN_CAPI_SEC:-0.05}"
WARN_FULL="${TKVM_BASELINE_WARN_FULL_SEC:-0.30}"
FAIL_CONTRACT="${TKVM_BASELINE_FAIL_CONTRACT_SEC:-0.05}"
FAIL_CAPI="${TKVM_BASELINE_FAIL_CAPI_SEC:-0.25}"
FAIL_FULL="${TKVM_BASELINE_FAIL_FULL_SEC:-1.00}"

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
		echo "Usage: $0 <phase-label> [--warn-contract <sec>] [--warn-capi <sec>] [--warn-full <sec>] [--fail-contract <sec>] [--fail-capi <sec>] [--fail-full <sec>]"
		exit 0
		;;
	*)
		echo "Unknown argument: $1"
		exit 2
		;;
	esac
done

echo "Running baseline capture for ${PHASE}"
"${SCRIPT_DIR}/capture_baseline.sh" "${PHASE}"

echo
echo "Applying drift gates"
"${SCRIPT_DIR}/compare_baselines.sh" \
	--warn-contract "${WARN_CONTRACT}" \
	--warn-capi "${WARN_CAPI}" \
	--warn-full "${WARN_FULL}" \
	--fail-contract "${FAIL_CONTRACT}" \
	--fail-capi "${FAIL_CAPI}" \
	--fail-full "${FAIL_FULL}"
