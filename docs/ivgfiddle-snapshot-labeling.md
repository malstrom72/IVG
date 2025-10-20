# ivgfiddle Snapshot Naming

The ivgfiddle toolbar populates its snapshot selector with entries derived from the catalog returned by the rasterizer. The logic lives in `tools/ivgfiddle/src/ivgfiddle.js` and follows these rules:

## Explicit scenarios
- A scenario is treated as explicit when `scenario.explicit === true` and it provides a non-empty `name`.
- Each entry coming from an explicit scenario uses that name as its base label.
- If the scenario produces multiple entries, ivgfiddle appends a list suffix (`#<n>`) where `n` is drawn from the first defined value among:
  1. `entry.listIndex`
  2. `entry.entryOrdinal - 1`
- Single-entry scenarios keep the base label without any list suffix.

## Implicit (unlabeled) scenarios
- Any scenario that is missing either `explicit === true` or a name falls into the implicit bucket.
- Implicit scenarios are grouped by `deriveImplicitGroupInfo`:
  - Names that already look like `something-<digits>` are split so that the shared prefix becomes the group key and the trailing digits (minus one) become a preferred list index.
  - Other named scenarios use their name as the group key.
  - Scenarios without a name use `implicit-<index>` where `<index>` is their catalog position (or `scenario.index` when present).
- `prepareImplicitGroups` orders the discovered groups by their first appearance in the catalog and assigns each one an `ordinal` starting at 1.
- The selector labels these scenarios as `unlabeled-<ordinal>` so the numbering always increases sequentially with the group order.

### List suffixes for implicit scenarios
When a grouped implicit scenario contributes more than one entry across the catalog:
1. The total number of entries routed through the group becomes the `entryCount`, ensuring every option for that scenario receives the same suffix logic.
2. The list index appended to `unlabeled-<ordinal>` comes from the first available source below:
   - The preferred index extracted by `deriveImplicitGroupInfo` (only used when the scenario yields a single entry but the original name supplied an index).
   - `entry.listIndex` when provided in the catalog.
   - `entry.entryOrdinal - 1` when the catalog offers ordinals but not list indices.
   - The running count of processed entries inside the group as a final fallback.
3. `buildOptionLabel` appends `#<n>` when `entryCount > 1`; single-entry groups emit just `unlabeled-<ordinal>`.

## Examples
### Explicit scenario with list entries
```
meta snapshot scenario:hero validate:yes list:[
[ size=19 ]
[ size=21 ]
]
```
The selector renders:
```
hero #0
hero #1
```

### Mixed explicit and implicit scenarios
```
format ivg-3 uses:snapshot-1
bounds 0,0,53,49
size=10
color=#808080
meta snapshot scenario:test validate:yes list:[
[ size=19 ], [ size=21 ]
]
meta snapshot scenario:test2 validate:yes [
color=#E74C3C
]
meta snapshot validate:yes list:[
[ color=#E74C3C ] [ size=44 ]
]
meta snapshot validate:yes list:[
[ color=#E74C3C ] [ size=44 ]
]
meta snapshot validate:yes [ size=66 ]
```
The selector renders:
```
test #0
test #1
test2
unlabeled-1 #0
unlabeled-1 #1
unlabeled-2 #0
unlabeled-2 #1
unlabeled-3
```

These rules mirror the behavior of the generated bundle in `tools/ivgfiddle/output/ivgfiddle.js`, keeping the snapshot picker consistent across development and published builds.
