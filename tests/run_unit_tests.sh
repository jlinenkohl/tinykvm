#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FOLDER="${SCRIPT_DIR}/build_unittests"

if [[ ! -f "${ROOT_DIR}/tests/Catch2/CMakeLists.txt" ]]; then
	echo "Missing tests/Catch2/CMakeLists.txt (Catch2 checkout/submodule not initialized)."
	echo "Skipping unit tests."
	exit 0
fi

mkdir -p "${FOLDER}"
pushd "${FOLDER}" >/dev/null
cmake ../unit -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j4
ctest --verbose "$@"
popd >/dev/null
