#!/bin/sh
set -eu

die() { echo "make_release: $*" >&2; exit 1; }

[ $# -eq 1 ] || die "usage: tools/make_release.sh <version>"
version=$1
case $version in
    ''|*[!0-9A-Za-z._-]*|[!0-9A-Za-z]*) die "version may only contain letters, digits, '.', '_' and '-', and must start with a letter or digit" ;;
esac

cd "$(dirname "$0")/.."
if command -v idf.py >/dev/null 2>&1; then
    idf_py=idf.py
elif [ -n "${IDF_PATH:-}" ] && [ -f "$IDF_PATH/tools/idf.py" ]; then
    idf_py="python $IDF_PATH/tools/idf.py"
else
    die "ESP-IDF is not active; activate ESP-IDF v6.1 first"
fi

[ -z "$(git status --porcelain)" ] || die "working tree is not clean; commit or stash your changes"
if git submodule status | grep -q '^[-+U]'; then
    die "a submodule is missing or is not at the commit recorded in this repository (see git submodule status)"
fi

$idf_py build
tools/run_tests.sh
python3 tools/check_safety.py --config sdkconfig

if python -m esptool merge-bin --help >/dev/null 2>&1; then
    merge=merge-bin
elif python -m esptool merge_bin --help >/dev/null 2>&1; then
    merge=merge_bin
else
    die "esptool is not available in the active ESP-IDF environment"
fi

image=dist/inkboy-$version.bin
app_image=dist/inkboy-$version-app.bin
[ -f build/inkboy.bin ] || die "build/inkboy.bin is missing"
mkdir -p dist
rm -f "$image" "$app_image"
cp build/inkboy.bin "$app_image"
(
    cd build
    python -m esptool --chip esp32s3 $merge -o "../$image" $(python3 -c '
import json
args = json.load(open("flasher_args.json"))
print(*args["write_flash_args"])
for offset, path in sorted(args["flash_files"].items(), key=lambda item: int(item[0], 16)):
    print(offset, path)
' | tr '\n' ' ')
)

if command -v sha256sum >/dev/null 2>&1; then sha256="sha256sum"; else sha256="shasum -a 256"; fi
(cd dist && $sha256 inkboy-*.bin > SHA256SUMS)

ls -l dist
cat dist/SHA256SUMS
