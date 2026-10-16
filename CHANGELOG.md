# Changelog

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

