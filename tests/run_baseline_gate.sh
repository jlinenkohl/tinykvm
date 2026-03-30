#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <phase-label> [--policy <balanced|strict|lenient>] [threshold overrides]"
	echo "Example: $0 phase13a --policy balanced --warn-full 0.30 --fail-full 1.00"
	exit 2
fi

PHASE="$1"
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

POLICY="${TKVM_BASELINE_POLICY:-balanced}"

apply_policy_defaults() {
	case "$1" in
	strict)
		WARN_CONTRACT="0.005"
		WARN_CAPI="0.03"
		WARN_FULL="0.20"
		FAIL_CONTRACT="0.02"
		FAIL_CAPI="0.15"
		FAIL_FULL="0.60"
		;;
	balanced)
		WARN_CONTRACT="0.01"
		WARN_CAPI="0.05"
		WARN_FULL="0.30"
		FAIL_CONTRACT="0.05"
		FAIL_CAPI="0.25"
		FAIL_FULL="1.00"
		;;
	lenient)
		WARN_CONTRACT="0.02"
		WARN_CAPI="0.10"
		WARN_FULL="0.60"
		FAIL_CONTRACT="0.10"
		FAIL_CAPI="0.40"
		FAIL_FULL="1.50"
		;;
	*)
		echo "Unknown policy: $1"
		echo "Supported policies: strict, balanced, lenient"
		exit 2
		;;
	esac
}

print_policies() {
	echo "Available policies:"
	echo "  strict   warn(contract=0.005,c_api=0.03,full=0.20) fail(contract=0.02,c_api=0.15,full=0.60)"
	echo "  balanced warn(contract=0.01,c_api=0.05,full=0.30) fail(contract=0.05,c_api=0.25,full=1.00)"
	echo "  lenient  warn(contract=0.02,c_api=0.10,full=0.60) fail(contract=0.10,c_api=0.40,full=1.50)"
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--policy)
		POLICY="${2:-}"
		shift 2
		;;
	--list-policies)
		print_policies
		exit 0
		;;
	*)
		break
		;;
	esac
done

apply_policy_defaults "${POLICY}"

# Explicit environment overrides keep CI/local behavior configurable.
WARN_CONTRACT="${TKVM_BASELINE_WARN_CONTRACT_SEC:-${WARN_CONTRACT}}"
WARN_CAPI="${TKVM_BASELINE_WARN_CAPI_SEC:-${WARN_CAPI}}"
WARN_FULL="${TKVM_BASELINE_WARN_FULL_SEC:-${WARN_FULL}}"
FAIL_CONTRACT="${TKVM_BASELINE_FAIL_CONTRACT_SEC:-${FAIL_CONTRACT}}"
FAIL_CAPI="${TKVM_BASELINE_FAIL_CAPI_SEC:-${FAIL_CAPI}}"
FAIL_FULL="${TKVM_BASELINE_FAIL_FULL_SEC:-${FAIL_FULL}}"

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
		echo "Usage: $0 <phase-label> [--policy <balanced|strict|lenient>] [--warn-contract <sec>] [--warn-capi <sec>] [--warn-full <sec>] [--fail-contract <sec>] [--fail-capi <sec>] [--fail-full <sec>]"
		print_policies
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
echo "Applying drift gates (policy: ${POLICY})"
echo "Thresholds: warn(contract=${WARN_CONTRACT}, c_api=${WARN_CAPI}, full=${WARN_FULL}) fail(contract=${FAIL_CONTRACT}, c_api=${FAIL_CAPI}, full=${FAIL_FULL})"
echo
"${SCRIPT_DIR}/compare_baselines.sh" \
	--warn-contract "${WARN_CONTRACT}" \
	--warn-capi "${WARN_CAPI}" \
	--warn-full "${WARN_FULL}" \
	--fail-contract "${FAIL_CONTRACT}" \
	--fail-capi "${FAIL_CAPI}" \
	--fail-full "${FAIL_FULL}"
