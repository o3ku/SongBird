# Python guards

Every rule in `scripts/check-*.ps1` has a port here, written against the standard library
only. The ports are what the CNB PR gate runs; the PowerShell originals are what Windows
developers and the GitHub release job run.

## Why two implementations

PowerShell cannot be installed on Debian any more: it lives only in Microsoft's apt source,
and that source's `InRelease` is signed with the same SHA1 self-signed key as its own package
metadata, which `sqv` rejects since 2026-02-01 — `apt-get update` exits 100. Adding the
source's key to `trusted.gpg.d` does not help, because the signature it would trust carries
the same rejected self-signature. So the PR gate ran none of these rules while the file
`.cnb.yml` had to say so in a comment.

Porting the rules costs a second implementation. Not running them costs the gate its only
static layer. The port was the cheaper of the two, and it does not touch the OS trust store.

## Keeping the two in step

Each port mirrors one script, rule for rule, and the comments explaining *why* a rule exists
live in the `.ps1` — the port does not restate them. When you change a rule, change both.
`selftest_guards.py` is the check on that: it breaks every rule in a throwaway copy and
asserts the port goes red, then asserts every port is green on an untouched copy.

`guard_equivalence.py` is the check on the other half of that promise: the self-test proves
each implementation can fail, the equivalence run proves the two implementations *agree*.
For every rule it runs the `.ps1` and the `.py` against the same clean tree and the same
mutations, and requires identical violation lists (path spellings normalized). It needs
both pwsh and python, so `py-guard-equivalence` is registered only where pwsh exists —
the GitHub Windows runners qualify, the Linux PR gate does not. Its first run already paid
for itself: `check-backend-boundaries.ps1` compared front-end directories by string, and
`Resolve-Path`'s 8.3 short-path form made every front end look uncovered on a temp-tree
copy — invisible while the guard only ever scanned `src` by relative path.

## What each one needs

| Test | Needs | Skips loudly without |
| --- | --- | --- |
| `py-backend-boundaries` | the source tree | — |
| `py-appbootstrap-member-order` | the source tree | — |
| `py-async-owners` | the source tree | — |
| `py-hardcoded-colors` | the source tree | — |
| `py-q-os-include-scope` | `src/` and `tests/` | — |
| `py-localization-coverage` | `src/`, the `.ts` | lupdate (Check B only) |
| `py-versions-consistent` | `CMakeLists.txt`, `src/`, `scripts/` | — |
| `py-readme-assets` | the READMEs, `docs/screenshots/` | — |
| `py-translations-fresh` | the `.ts`/`.qm` pair | lrelease (the byte-exact half only) |
| `py-translations-embedded` | a build tree | not registered without one |
| `py-test-suites-nonempty` | a build tree full of test binaries | not registered without one |
| `py-guard-selftest` | the source tree | — |

"N skips loudly" means the guard prints what it did **not** check. A check that silently
does less is the failure mode this whole directory exists to prevent.

## Running them by hand

```
python3 scripts/python/run_all_guards.py     # every rule, one line each
python3 scripts/python/selftest_guards.py    # prove the rules can still go red
```

Both work with no cmake, no Qt and no network.
