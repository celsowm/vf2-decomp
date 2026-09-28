# `fa_coli` `0x225cc`: high returned type-22 selectors v0536

The complete live selector sweep `g8+0x19c == 1..1024` remains unchanged.
Additional oracle probes from `out/coli-225cc-entry.vf2snap` used
`g8+0x19f = 22`, the live `g8 = 0x00512980` base and stop address `0x10dcc`.
In the mixed interval `1345..1408`, 39 selectors reached the parent in the
reference. The existing C walker and shortcut match 35 of those returned
cases with complete CPU/condition/procedure/Model 2A state equality:

```text
1345..1359, 1363..1364, 1366..1369, 1373,
1380..1381, 1383..1386, 1391..1392, 1397..1398, 1404, 1407..1408
```

The exact admitted set is pinned by the fixture rather than by a contiguous
range. The four returned cases `1361`, `1378`, `1395` and `1405` currently
reach the reference parent but hit unsupported record-chain shapes in the C
walker, so they remain outside the native evidence. The other 25 selectors in
the interval do not reach `0x10dcc` in the reference (walker loop or memory
fault) and remain fail-closed as well.

No new general selector rule is inferred from this mixed region.
