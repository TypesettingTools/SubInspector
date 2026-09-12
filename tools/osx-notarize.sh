#!/bin/sh
set -eu

if test "$#" -ne 2; then
  echo "Usage: $0 ARTIFACT_DIR OUTPUT_ZIP" >&2
  exit 1
fi
if test -z "${SUBINSPECTOR_NOTARY_PROFILE:-}"; then
  echo "Set SUBINSPECTOR_NOTARY_PROFILE to a notarytool Keychain profile" >&2
  exit 1
fi

ARTIFACT_DIR=$(cd "$1" && pwd)
OUTPUT_DIR=$(cd "$(dirname "$2")" && pwd)
OUTPUT_ZIP="${OUTPUT_DIR}/$(basename "$2")"
case "${OUTPUT_ZIP}" in
  *.zip) ;;
  *) echo "OUTPUT_ZIP must end in .zip" >&2; exit 1 ;;
esac
if test -e "${OUTPUT_ZIP}"; then
  echo "${OUTPUT_ZIP} already exists" >&2
  exit 1
fi

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/subinspector-notary.XXXXXX")
trap 'rm -rf "${WORK_DIR}"' EXIT
trap 'exit 1' HUP INT TERM

# Validate a private copy, then submit exactly those files.
PACKAGE_DIR="${WORK_DIR}/$(basename "${ARTIFACT_DIR}")"
ditto "${ARTIFACT_DIR}" "${PACKAGE_DIR}"
(
  cd "${PACKAGE_DIR}"
  grep -Eq '^[[:xdigit:]]{64}  libSubInspector[.]dylib$' SHA256SUMS
  shasum -a 256 -c SHA256SUMS
  codesign --verify --strict --verbose=2 \
    -R='anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.13] exists' \
    libSubInspector.dylib
  SIGNATURE=$(codesign --display --verbose=4 libSubInspector.dylib 2>&1)
  if ! printf '%s\n' "${SIGNATURE}" | grep -q '^Timestamp='; then
    echo "The dylib has no secure signing timestamp" >&2
    exit 1
  fi
  if ! printf '%s\n' "${SIGNATURE}" | grep -q '^CodeDirectory .*flags=.*runtime'; then
    echo "The dylib was not signed with --options runtime" >&2
    exit 1
  fi
)
ARCHIVE="${WORK_DIR}/submission.zip"
ditto -c -k --keepParent "${PACKAGE_DIR}" "${ARCHIVE}"

run_notarytool() {
  command=$1
  shift
  if test -n "${SUBINSPECTOR_NOTARY_KEYCHAIN:-}"; then
    xcrun notarytool "${command}" --keychain-profile "${SUBINSPECTOR_NOTARY_PROFILE}" \
      --keychain "${SUBINSPECTOR_NOTARY_KEYCHAIN}" "$@"
  else
    xcrun notarytool "${command}" --keychain-profile "${SUBINSPECTOR_NOTARY_PROFILE}" "$@"
  fi
}

RESULT="${WORK_DIR}/result.plist"
if run_notarytool submit "${ARCHIVE}" --wait \
    --timeout "${SUBINSPECTOR_NOTARY_TIMEOUT:-30m}" --output-format plist > "${RESULT}"; then
  SUBMIT_EXIT=0
else
  SUBMIT_EXIT=$?
fi
if test -s "${RESULT}"; then
  plutil -p "${RESULT}" || cat "${RESULT}"
fi
SUBMISSION_ID=$(plutil -extract id raw -o - "${RESULT}" 2>/dev/null || true)
STATUS=$(plutil -extract status raw -o - "${RESULT}" 2>/dev/null || true)
if test "${SUBMIT_EXIT}" -ne 0 || test "${STATUS}" != Accepted; then
  if test -n "${SUBMISSION_ID}"; then
    run_notarytool log "${SUBMISSION_ID}" || true
  fi
  echo "Notarization failed with status ${STATUS:-unknown}" >&2
  exit 1
fi

# Apple issues tickets for dylibs, but neither dylibs nor ZIPs support stapling.
mv "${ARCHIVE}" "${OUTPUT_ZIP}"
shasum -a 256 "${OUTPUT_ZIP}"
echo "Notarized ${OUTPUT_ZIP}"
