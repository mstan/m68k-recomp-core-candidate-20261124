# Private component qualification

Standalone Windows native build and 5 CTest cases pass. Evidence is retained privately under shared-core extraction campaign `beads-69le5`. This qualifies extracted component contracts, not system integration, gameplay, audio quality or a performance improvement. Production consumers remain unchanged.

Dedicated-core baseline: `ddae973072ce7ec56407a89d1d7a3e6a94e1487e`.

## SCC68070 reconciliation

The SCC68070 profile follows `cd1e5e28dc26ec6a887197aac7eaaf8c8c870019`
and the register-indexed table discovery correction from
`2dbf4886129405e2bc507202d9fe5066075b272a`. Common code, Genesis generation,
portable timing/step runtimes and their interfaces retain the qualified
`36fe6fbe1edeb454fe168f948716e0c6fdb33c59` implementation.

The CD-i synthetic frontend regressions pass, including positive and negative
register-table cases. All 353 BIOS/module files emitted by the reconciled
frontend match the current CD-i profile output byte for byte. The title
driver's game identity header is outside that emission comparison. These
checks qualify frontend compatibility; game-route acceptance remains separate.
