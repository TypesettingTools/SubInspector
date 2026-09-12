# macOS release signing

Use the same Developer ID Application identity as Aegisub: its library
validation requires native Automation libraries to have the same Team ID.
These scripts use downloaded CI artifacts without rebuilding dependencies.
They require macOS, Xcode command-line tools, your signing certificate, and
a stored `notarytool` profile. Signing and notarization need network access.

Check out the commit that produced the artifacts, then download both macOS
packages from its successful CI run:

```sh
gh run download RUN_ID --name SubInspector-macos-arm64 --dir build/SubInspector-macos-arm64
gh run download RUN_ID --name SubInspector-macos-x86_64 --dir build/SubInspector-macos-x86_64

export SUBINSPECTOR_SIGNATURE='Developer ID Application: Your Name (TEAMID)'
export SUBINSPECTOR_TEAM_ID=TEAMID
export SUBINSPECTOR_NOTARY_PROFILE=aegisub-notary
```

You can reuse Aegisub's existing notary profile. To create a new one, run
`xcrun notarytool store-credentials "$SUBINSPECTOR_NOTARY_PROFILE"` once.
For non-default keychains, set `SUBINSPECTOR_SIGNING_KEYCHAIN` and/or
`SUBINSPECTOR_NOTARY_KEYCHAIN`. `SUBINSPECTOR_NOTARY_TIMEOUT` defaults to `30m`.
Set `SUBINSPECTOR_TEAM_ID` to the 10-character `TeamIdentifier` shown by
`codesign --display --verbose=4 /path/to/Aegisub.app`. Notarization verifies
that the dylib is signed by that team before submitting it to Apple.

```sh
for arch in arm64 x86_64; do
  artifact="build/SubInspector-macos-$arch"
  tools/osx-sign.sh "$artifact"
  tools/osx-notarize.sh "$artifact" "$artifact.zip"
done
```

Signing verifies the downloaded checksums, signs the dylib with a secure
timestamp, and atomically replaces `SHA256SUMS`. The notarization script checks
the signed package and produces the requested ZIP only after Apple returns `Accepted`.
The ZIP's top-level folder matches its filename without `.zip`, regardless of
the input directory's name. It refuses to overwrite an existing output, including
one created while awaiting notarization. On failure it prints the submission
ID/status and requests Apple's log when an ID is available; rerun after fixing
the issue. `SUBINSPECTOR_SIGNATURE=-` permits local ad-hoc signing tests, but
those artifacts cannot pass the notarization script's Developer ID check.

Test the signed dylibs through the wrapper in the signed Aegisub release.
Publish the accepted ZIPs and use the **signed** dylibs when updating the
DependencyControl feed's download URLs and hashes. Keep the wrapper version,
changelog, and dependency changes together with that feed update.

Standalone dylibs and ZIPs cannot have notarization tickets stapled to them;
Apple records tickets online. See [Apple's notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow).
