#!/bin/sh
set -eu

if test "$#" -ne 1; then
  echo "Usage: $0 ARTIFACT_DIR" >&2
  exit 1
fi
if test -z "${SUBINSPECTOR_SIGNATURE:-}"; then
  echo "Set SUBINSPECTOR_SIGNATURE to Aegisub's Developer ID identity (or '-' for an ad-hoc test)" >&2
  exit 1
fi

cd "$1"
LIBRARY=libSubInspector.dylib
if ! grep -Eq '^[[:xdigit:]]{64}  libSubInspector[.]dylib$' SHA256SUMS; then
  echo "Expected a macOS CI artifact with libSubInspector.dylib in SHA256SUMS" >&2
  exit 1
fi
shasum -a 256 -c SHA256SUMS

if test "${SUBINSPECTOR_SIGNATURE}" = '-'; then
  codesign --force --sign - "${LIBRARY}"
else
  set -- --force --options runtime --timestamp --sign "${SUBINSPECTOR_SIGNATURE}"
  if test -n "${SUBINSPECTOR_SIGNING_KEYCHAIN:-}"; then
    set -- "$@" --keychain "${SUBINSPECTOR_SIGNING_KEYCHAIN}"
  fi
  codesign "$@" "${LIBRARY}"
fi
codesign --verify --strict --verbose=2 "${LIBRARY}"

# Signing changes only the dylib. Preserve the checksums of the other files.
CHECKSUMS=$(mktemp './.SHA256SUMS.XXXXXX')
trap 'rm -f "${CHECKSUMS}"' EXIT
trap 'exit 1' HUP INT TERM
SIGNED_CHECKSUM=$(shasum -a 256 "${LIBRARY}")
awk -v signed="${SIGNED_CHECKSUM}" '
  $2 == "libSubInspector.dylib" { $0 = signed }
  { print }
' SHA256SUMS > "${CHECKSUMS}"
chmod 644 "${CHECKSUMS}"
mv -f "${CHECKSUMS}" SHA256SUMS
shasum -a 256 -c SHA256SUMS
echo "Signed ${LIBRARY} and updated SHA256SUMS"
