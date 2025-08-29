#!/bin/sh
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$DIR/.."

node "$DIR/parseQuickHashGenInput.test.js"
node "$DIR/parseCString.test.js"
node "$DIR/cli.test.js"
node "$DIR/search.test.js"
node "$DIR/helpers.test.js"

node "$ROOT/QuickHashGenCLI.node.js" --seed 1 --tests 100 "$DIR/input1.txt" > "$DIR/out1.c"
# help flag should print usage and succeed
node "$ROOT/QuickHashGenCLI.node.js" --help >"$DIR/help.log" 2>&1
grep -q 'Usage:' "$DIR/help.log"
rm "$DIR/help.log"
node "$ROOT/QuickHashGenCLI.node.js" -h >"$DIR/help.log" 2>&1
grep -q 'Usage:' "$DIR/help.log"
rm "$DIR/help.log"
# invalid option values should print usage and fail
if node "$ROOT/QuickHashGenCLI.node.js" --tests -1 "$DIR/input1.txt" >/dev/null 2>"$DIR/err.log"; then
echo "Expected failure for --tests -1" >&2
rm "$DIR/err.log"
exit 1
fi
grep -q 'Usage:' "$DIR/err.log"
rm "$DIR/err.log"

if node "$ROOT/QuickHashGenCLI.node.js" --seed foo "$DIR/input1.txt" >/dev/null 2>"$DIR/err.log"; then
echo "Expected failure for --seed foo" >&2
rm "$DIR/err.log"
exit 1
fi
grep -q 'Usage:' "$DIR/err.log"
rm "$DIR/err.log"

# verify option handling
node "$ROOT/QuickHashGenCLI.node.js" --seed 1 --tests 100 --force-eval --eval-test "$DIR/input1.txt" > "$DIR/out1_eval.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 123 --tests 100 "$DIR/input2.txt" > "$DIR/out2.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 10 --tests 50000 "$DIR/input3.txt" > "$DIR/out3.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 7 --tests 1000 "$DIR/input4.txt" > "$DIR/out4.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 11 --tests 2000 "$DIR/input5.txt" > "$DIR/out5.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 13 --tests 100000 "$DIR/input6.txt" > "$DIR/out6.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 17 --tests 500 "$DIR/input7.txt" > "$DIR/out7.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 19 --tests 2000 "$DIR/input8.txt" > "$DIR/out8.c"
node "$ROOT/QuickHashGenCLI.node.js" --seed 23 --tests 100000 "$DIR/input9.txt" > "$DIR/out9.c"

diff -u "$DIR/out1.c" "$DIR/out1_eval.c" || { echo "Diff found: $DIR/out1.c vs $DIR/out1_eval.c" >&2; exit 1; }
diff -u "$DIR/golden1.c" "$DIR/out1.c" || { echo "Diff found: $DIR/golden1.c vs $DIR/out1.c" >&2; exit 1; }
rm "$DIR/out1.c" "$DIR/out1_eval.c"

diff -u "$DIR/golden2.c" "$DIR/out2.c" || { echo "Diff found: $DIR/golden2.c vs $DIR/out2.c" >&2; exit 1; }
rm "$DIR/out2.c"

diff -u "$DIR/golden3.c" "$DIR/out3.c" || { echo "Diff found: $DIR/golden3.c vs $DIR/out3.c" >&2; exit 1; }
rm "$DIR/out3.c"

diff -u "$DIR/golden4.c" "$DIR/out4.c" || { echo "Diff found: $DIR/golden4.c vs $DIR/out4.c" >&2; exit 1; }
rm "$DIR/out4.c"

diff -u "$DIR/golden5.c" "$DIR/out5.c" || { echo "Diff found: $DIR/golden5.c vs $DIR/out5.c" >&2; exit 1; }
rm "$DIR/out5.c"

diff -u "$DIR/golden6.c" "$DIR/out6.c" || { echo "Diff found: $DIR/golden6.c vs $DIR/out6.c" >&2; exit 1; }
rm "$DIR/out6.c"

diff -u "$DIR/golden7.c" "$DIR/out7.c" || { echo "Diff found: $DIR/golden7.c vs $DIR/out7.c" >&2; exit 1; }
rm "$DIR/out7.c"

diff -u "$DIR/golden8.c" "$DIR/out8.c" || { echo "Diff found: $DIR/golden8.c vs $DIR/out8.c" >&2; exit 1; }
rm "$DIR/out8.c"

diff -u "$DIR/golden9.c" "$DIR/out9.c" || { echo "Diff found: $DIR/golden9.c vs $DIR/out9.c" >&2; exit 1; }
rm "$DIR/out9.c"

# Exercise the README example (default 100000 tests) to ensure it completes
printf "black\nsilver\ngray\nwhite\nmaroon\nred\npurple\nfuchsia\ngreen\nlime\nolive\nyellow\nnavy\nblue\nteal\naqua\n" | node "$ROOT/QuickHashGenCLI.node.js" > "$DIR/colors.c"
test -s "$DIR/colors.c"
rm "$DIR/colors.c"

echo "All tests passed."
