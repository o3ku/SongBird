#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

#include "common/OperationResult.h"

// SHA-256 verification for anything downloaded from a GitHub release: the app update, core
// packages and geo databases. The trust anchor is always GitHub itself -- either the `digest`
// field the releases API returns for each asset, or a checksum asset published in the same
// release. A checksum fetched from anywhere else cannot vouch for the bytes, so there is no
// third source here.
//
// Verification is fail-closed by default: `verifyDownloadedBytes` reports Unverifiable when
// no expected hash can be established at all, and callers refuse the install. The one exception
// is a locally constructed release (the built-in core bootstrap, used when no core is installed
// and the releases API is unreachable): there is no release metadata by definition, so the
// caller warns loudly and proceeds instead of bricking a first run. That exception is the
// caller's decision, not this helper's -- the helper never returns "verified" without a hash.
namespace ReleaseChecksum {

// SHA-256 hex (lowercase) of `data`.
QString sha256Hex(const QByteArray& data);

// Normalises a GitHub API digest ("sha256:<hex>") or a bare hex string to lowercase hex.
// Returns empty for anything else.
QString digestToHex(const QString& digest);

// True when `data` hashes to `expectedDigest` ("sha256:<hex>" or bare hex, any case).
bool matchesSha256(const QByteArray& data, const QString& expectedDigest);

// A release asset, reduced to what verification needs. Callers map their own asset structs
// onto this instead of this helper knowing three release-metadata types.
struct NamedAsset {
    QString name;
    QUrl downloadUrl;
};

// The checksum asset for `assetName` among a release's assets, if the release publishes one:
// "<asset>.sha256", "<asset>.dgst", or a "checksums.txt" that lists the asset.
std::optional<NamedAsset> findChecksumAsset(
    const QList<NamedAsset>& assets,
    const QString& assetName);

// The expected SHA-256 hex for `assetName` inside a checksum document, if the document names
// it. Understands the formats upstreams actually publish:
//   <hex>  <filename>            (sha256sum / goreleaser checksums.txt)
//   <hex>                        (a file holding a single hash)
//   SHA256 (filename) = <hex>    (openssl dgst, as Xray publishes *.dgst)
//   SHA2-256 (filename) = <hex>  (newer openssl spelling; the filename part is optional)
// A filename is matched on its basename, so a document listing a path still matches.
std::optional<QString> expectedSha256ForAsset(const QByteArray& document, const QString& assetName);

// Checksum URLs derived from a package download URL, in the order to try them:
// "<package>.sha256", "<package>.dgst", then "checksums.txt" in the same directory.
QList<QUrl> derivedChecksumUrls(const QUrl& packageDownloadUrl);

enum class Verification {
    Verified,
    Mismatched,
    Unverifiable,
};

struct VerificationOutcome {
    Verification status = Verification::Unverifiable;
    // Lowercase hex. Set for Verified and Mismatched; empty for Unverifiable.
    QString expectedHex;
    QString actualHex;
};

// Verifies `packageBytes` for `assetName`, in order:
//   1. `apiDigest`, when the release metadata carried one;
//   2. `checksumAsset`, downloaded with `fetchChecksum` and parsed for the asset's hash;
//   3. the checksum URLs derived from `packageDownloadUrl`, tried in order.
// `fetchChecksum` is (url, content) -> OperationResult, the caller's own download path.
// Returns Mismatched (never Verified) when a hash was established and the bytes differ, and
// Unverifiable when no hash could be established from any source.
VerificationOutcome verifyDownloadedBytes(
    const QByteArray& packageBytes,
    const QString& assetName,
    const QUrl& packageDownloadUrl,
    const QString& apiDigest,
    const std::optional<NamedAsset>& checksumAsset,
    const std::function<OperationResult(const QUrl& url, QByteArray* content)>& fetchChecksum);

} // namespace ReleaseChecksum
