# Changelog

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

