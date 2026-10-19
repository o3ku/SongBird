# Changelog

## 2.7.0 — The system proxy is now something we restore, not something we erase

- Fix: quitting no longer deletes a system proxy the user had configured before starting SongBird. `WindowsSystemProxyService` captures `ProxyEnable`/`ProxyServer`/`ProxyOverride`/`AutoConfigURL` before the first write and puts them back afterwards; a key that did not exist is removed, a key that existed is restored even when its value was empty. Previously `resetOnShutdown()` removed both keys outright.
- Fix: a failed startup no longer clears the system proxy unless this session is what applied it. `cleanupAfterFailedStartup()` used to clear when the configuration asked for a proxy **or** the registry happened to be enabled — both true for a user who was running their own proxy, so one failed startup deleted their settings. The rule now lives in `appcore/SystemProxyOwnershipPolicy.h` and keys off ownership alone.
- Fix: the proxy snapshot survives a killed process. It is written to the `.state.json` companion at capture time, so the next launch can restore the user's values rather than only recognising our own leftovers.
- Change: a snapshot that points at our own port (the crashed-session case) restores to "proxy off" instead of re-applying a dead endpoint.
- Change: `ISystemProxyService` gained `capture()`/`restore()`; macOS reports `capture()` as invalid deliberately, because its per-network-service settings cannot be re-applied faithfully, and the callers keep the previous clear-on-exit behaviour there.

- Fix: subscription updates now use conditional GET (ETag / If-None-Match / Last-Modified / If-Modified-Since). A 304 Not Modified response short-circuits the update without replacing the server list. The subscription's `etag`, `lastModified`, and `failStreak` fields are persisted to the config file after each attempt.
- Fix: a transient subscription download failure is retried up to 3 times with back-off (300 ms, 800 ms) before reporting failure. Each retry responds to shutdown interruption so the app can still exit cleanly mid-retry.
- Fix: `failStreak` is incremented and saved after the last failed attempt; it is reset to 0 after any success (including 304). This gives the UI a signal to de-emphasise subscriptions with repeated failures without blocking access to them.

- Fix: orphaned core processes no longer survive a parent crash. Every core child is assigned to a Windows Job Object with `KILL_ON_JOB_CLOSE` on `QProcess::started`; when this process dies by any means the OS terminates all job members. The existing PID-file / `CoreProcessCleanupService` path is kept as a fallback for processes that escape assignment (e.g. already in an incompatible job).

- Dev: `scripts/selftest-guards.ps1` proves the architecture guards can still fail — it breaks each rule in a throwaway copy of `src/` and asserts a red exit, then asserts green on the untouched tree.
- Dev: `scripts/check-async-owners.ps1` rejects a worker-thread continuation that captures `this` without a lifetime token, and an `invokeMethod` posted elsewhere while capturing `this`.
- Dev: `scripts/check-hardcoded-colors.ps1` keeps colour literals inside the two palette files.
- Dev: `scripts/check-test-suites.ps1` (run with `ctest -L suite`) treats a suite that ran zero test functions as a failure. Qt Test counts `initTestCase`/`cleanupTestCase` as passes, so such a suite otherwise reports `Totals: 2 passed` and a green build.
- Dev: SongBirdAuto's stylesheet now interpolates `auto/AutoWindowPalette.cpp` instead of carrying 19 hex literals inline; the rendered stylesheet is byte-identical.

- Feature: exporting servers is now a batch operation. `services/ExportService` produces share URIs or a Mihomo/Clash `proxies:` block for any list of nodes, and a dedup pass drops nodes that the same endpoint advertises twice under different names.
- Feature: `subscription/NodeFingerprint.h` keys a node on protocol, address, port and credentials only, so a server imported from two subscriptions is recognised as one. This is intentionally coarser than `SubscriptionService::serverReuseKey()`, which stays fine-grained so a refresh can carry a saved test result across.
- Change: `subscription/ClashYamlBuilder` writes the Clash YAML that `ClashYamlProxyParser` reads, so a node exported as YAML and re-imported comes back with the same address, port, credentials and TLS settings. Nodes with no Clash equivalent (Custom, Unknown) and nodes with an empty address or invalid port are skipped rather than emitted broken.
- Dev: `check-localization-coverage.ps1` now allowlists the emitted `proxies: []` YAML key, which is serialised config rather than a sentence a translator should touch.

- Feature: auto node selection now ranks by a score instead of the raw latency of the last probe. `appcore/RoutingScorePolicy` smooths latency with an EWMA so one outlier cannot reorder the list, trips a circuit breaker after three consecutive failures so a node that is currently failing cannot win on a stale good reading, and requires a challenger to be 25 % better before the active node is replaced so two nodes within noise of each other do not churn the connection.
- Change: `bestAutoServerIdForCountry()` now delegates to `bestAutoServerIdForCountryByScore()` with the raw latencies as scores, so the two selection rules can no longer drift apart. Behaviour is unchanged for a single probe; the difference only shows once `RoutingScorePolicy` state exists.
- Fix: a node with no recorded score is no longer a candidate, which stops a freshly imported node from being picked ahead of one that has actually been measured.

- Feature: servers can be exported in batch from the context menu, as share links or as a Mihomo/Clash `proxies:` block. The window owns the save dialog and the write; `services/ExportService` owns the formatting, so a new format is a new provider rather than a new dependency from `ui/` into `services/`.
- Feature: subscription updates walk the batch in a deliberate order. Subscriptions sharing a host are spread across round-robin passes so one host does not see a burst, and subscriptions with a higher `failStreak` are deferred so a dead URL does not delay the healthy ones. The loop stays serial on purpose — `config` is shared mutable state.
- Dev: a terminal-proxy service manages `HTTP_PROXY`/`HTTPS_PROXY`/`ALL_PROXY`/`NO_PROXY`, which is what curl, git, npm and pip read and what the WinINET system proxy does not cover. Writing updates both the registry and the in-process environment, then broadcasts `WM_SETTINGCHANGE` with `SendMessageTimeoutW` so a hung window cannot block it. `NO_PROXY` is only written when the user has none, and is never deleted on disable. **Not wired to the UI yet** — no settings switch, no call on proxy start or stop — so it has no user-visible effect in this release; it ships as a library plus tests so the dangerous half is verified before a switch exists.
- Dev: `appcore/HealthStripPolicy` folds probe records into a 24-hour strip (`+` all passed, `-` any failure, `0` untested), with worst-in-hour winning. Records outside the window — too old, or dated in the future — are dropped rather than clamped. **Not wired to the UI yet**: nothing feeds it probe records and the server table has no strip column.
- Fix: the release tag is now checked against the version the binary reports. Tagging without bumping `project(VERSION)` used to publish a release whose About dialog, update check and release page disagreed; `scripts/check-version-consistency.ps1` fails the workflow instead, and the same script runs in `ctest` for the in-tree half.
- Dev: `selftest-guards.ps1` covers the version check too — it breaks the composition and passes a mismatched tag, asserting red both times.

## 2026-10-16 — Follow-system theme, clearer DNS and Core settings (2.6.0)

- Fix: "Follow system" is now actually persisted. It used to be folded to Light while comparing settings, so the apply plan reported "nothing changed" and the choice was lost on restart.
- Fix: The server table's Result column shows a colour band per latency instead of bare text, and falls back to a failure band for results it cannot interpret ("Timeout", "Blocked", raw errors).
- Fix: An empty server table now explains what to do next, and names the filter box as the cause when a filter is hiding the servers.
- Feature: Core downloads show a progress bar and can be cancelled. Cancellation is a flag the worker polls at its own cancellation points rather than killing the thread mid-write.
- Feature: The six DNS options that only one core reads moved to the Core page under the core that honours them, instead of appearing to apply to every core while doing nothing for the others.
- Feature: The three DNS address fields explain what each answers, and Bootstrap DNS says "not needed" while the servers above it are IPs.
- Change: DNS defaults are IP form (`https://1.1.1.1/dns-query`, `https://223.5.5.5/dns-query`) rather than hostnames, so they no longer depend on a hard-coded hosts snapshot that can go stale.
- Change: Input padding on QLineEdit/QComboBox/QSpinBox tightened from 8px to 4px horizontally.

Notes:
- The sing-box-only "Global FakeIP" stays gated by the shared FakeIP checkbox on the DNS page; sing-box and Mihomo both honour FakeIP, Xray does not.
- The keyboard-shortcut list moved into its own dialog so the advertised shortcuts cannot drift from what the window binds.

## 2026-10-16 — Trojan default TLS & Xray TLS compatibility (PR #2)

- Fix: Ensure Trojan import and legacy saved configs default to TLS when the `security` field is missing or empty, preserving explicit values such as `none` or `reality`.
- Fix: Add runtime Xray version detection and a compatibility adapter that:
  - Rejects `allowInsecure: true` on Xray versions that no longer support skipping TLS verification.
  - Normalizes and validates `pinnedPeerCertSha256` formats and separators per Xray version differences.
  - Preserves existing runtime configuration on compatibility failures and reports incompatible nodes during batch speed tests instead of failing the whole batch.
- Tests: Add unit tests covering version boundaries, pin formatting, import/serialization behavior, and batch/single speed test handling.
- Docs: Add Xray TLS compatibility notes and update Chinese translations for new messages.

Notes:
- This change affects how Trojan links without an explicit `security` are interpreted (they now default to TLS).
- Xray compatibility behavior depends on the actual Xray executable version used at runtime; see docs/Xray-TLS兼容说明.md for full details.

