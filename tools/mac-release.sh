#!/bin/bash
# Signs, packages and notarizes a plugin (Lumen by default, PLUGIN=Chroma for the compressor) for macOS.
#
#   PLUGIN=Lumen \
#   DEV_ID_APP="Developer ID Application: Name (TEAMID)" \
#   DEV_ID_INSTALLER="Developer ID Installer: Name (TEAMID)" \
#   NOTARY_PROFILE=rba-notary \
#   tools/mac-release.sh build/Lumen_artefacts/Release      (Chroma: compressor/build/Chroma_artefacts/Release)
#
# NOTARY_PROFILE is a keychain profile made once with
#   xcrun notarytool store-credentials rba-notary --key <AuthKey.p8> --key-id <ID> --issuer <ISSUER>
# The AAX is included only when WRAPTOOL (PACE Eden wraptool) and PACE_ACCOUNT are set: Pro Tools
# needs a PACE-signed AAX, and wraptool applies the Developer ID signature itself.
set -euo pipefail

PLUGIN="${PLUGIN:-Lumen}"
ART="${1:?usage: mac-release.sh <${PLUGIN}_artefacts/Release>}"
: "${DEV_ID_APP:?}" "${DEV_ID_INSTALLER:?}" "${NOTARY_PROFILE:?}"
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$ART/VST3/$PLUGIN.vst3/Contents/Info.plist")
OUT="$(pwd)/dist"; WORK=$(mktemp -d)
mkdir -p "$OUT"
trap 'rm -rf "$WORK"' EXIT

sign() { codesign --force --timestamp --options runtime --sign "$DEV_ID_APP" "$1"; codesign --verify --strict --deep "$1"; }

mkdir -p "$WORK/vst3" "$WORK/au"
cp -R "$ART/VST3/$PLUGIN.vst3" "$WORK/vst3/"; sign "$WORK/vst3/$PLUGIN.vst3"
cp -R "$ART/AU/$PLUGIN.component" "$WORK/au/"; sign "$WORK/au/$PLUGIN.component"

ID=com.razburgaudiocrafts.$(echo "$PLUGIN" | tr "[:upper:]" "[:lower:]")
pkgbuild --root "$WORK/vst3" --install-location "/Library/Audio/Plug-Ins/VST3" --identifier $ID.vst3 --version "$VERSION" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/au" --install-location "/Library/Audio/Plug-Ins/Components" --identifier $ID.au --version "$VERSION" "$WORK/au.pkg"
CHOICES='<choice id="vst3" title="VST3"><pkg-ref id="'$ID'.vst3"/></choice><choice id="au" title="Audio Unit"><pkg-ref id="'$ID'.au"/></choice>'
OUTLINE='<line choice="vst3"/><line choice="au"/>'
REFS='<pkg-ref id="'$ID'.vst3">vst3.pkg</pkg-ref><pkg-ref id="'$ID'.au">au.pkg</pkg-ref>'

if [[ -n "${WRAPTOOL:-}" && -n "${PACE_ACCOUNT:-}" ]]; then
    mkdir -p "$WORK/aax"
    "$WRAPTOOL" sign --verbose --account "$PACE_ACCOUNT" ${PACE_WCGUID:+--wcguid "$PACE_WCGUID"} \
        --signid "$DEV_ID_APP" --in "$ART/AAX/$PLUGIN.aaxplugin" --out "$WORK/aax/$PLUGIN.aaxplugin"
    pkgbuild --root "$WORK/aax" --install-location "/Library/Application Support/Avid/Audio/Plug-Ins" --identifier $ID.aax --version "$VERSION" "$WORK/aax.pkg"
    CHOICES+='<choice id="aax" title="AAX (Pro Tools)"><pkg-ref id="'$ID'.aax"/></choice>'
    OUTLINE+='<line choice="aax"/>'; REFS+='<pkg-ref id="'$ID'.aax">aax.pkg</pkg-ref>'
else
    echo "note: WRAPTOOL/PACE_ACCOUNT not set, building without AAX" >&2
fi

cat > "$WORK/dist.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$PLUGIN $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>$OUTLINE</choices-outline>
  $CHOICES
  $REFS
</installer-gui-script>
XML

PKG="$OUT/$PLUGIN-$VERSION-mac.pkg"
productbuild --distribution "$WORK/dist.xml" --package-path "$WORK" "$WORK/unsigned.pkg"
productsign --timestamp --sign "$DEV_ID_INSTALLER" "$WORK/unsigned.pkg" "$PKG"
xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$PKG"
spctl --assess --type install -vv "$PKG"
shasum -a 256 "$PKG"
echo "done: $PKG"
