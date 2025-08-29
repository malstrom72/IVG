set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
tmpdir="$(mktemp -d)"
trap "rm -rf '$tmpdir'" EXIT

seed=1
while true
do
echo "seed $seed"
node "$DIR/QuickHashGenTest.node.js" "$seed" >"$tmpdir/test.cpp"
g++ -o "$tmpdir/test" "$tmpdir/test.cpp"
"$tmpdir/test"
seed=$((seed+1))
done
