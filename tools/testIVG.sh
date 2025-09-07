#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
if [ -z "${1-}" ]; then
	EXE="../output/IVG2PNG"
else
	EXE=$1
fi
FONTS=../fonts
IMAGES=.

tmp=$(mktemp -d)
echo Using temporary dir: "$tmp"

fail=0
set +e
for f in ./ivg/*.ivg; do
    n=${f#./ivg/}
    n=${n%.ivg}
    echo Doing "$n"
    echo
    args=""
    if [ "$n" = "huge" ]; then
        args="--fast"
    fi
    $EXE $args --images "$IMAGES" --fonts "$FONTS" "$f" "$tmp/$n.png"
    if [ $? -ne 0 ]; then
        fail=1
        echo
        echo
        continue
    fi
    if [ "$UPDATE" -eq 1 ]; then
        cp "$tmp/$n.png" "./png/$n.png"
    else
        cmp "$tmp/$n.png" "./png/$n.png"
        [ $? -ne 0 ] && fail=1
    fi
    echo
    echo
done
set -e
rm -rf "$tmp"/*.png
rmdir "$tmp"
exit $fail
