#!/bin/bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

EXE=${1:-../output/IVG2PNG}

for f in ivg/*.ivg; do
        n=${f#ivg/}
        n=${n%.ivg}
        echo Doing "$n"
        echo
        "$EXE" "$f" "png/$n.png"
        echo
        echo
done

