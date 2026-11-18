#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

#include "common/OperationResult.h"

// SHA-256 verification for anything downloaded from a GitHub release: the app update, core
// packages and geo databases.
//
// Trust model. A checksum is only an anchor when it arrived over the official GitHub
// channel, and there are exactly two ways that happens:
//   - the releases API answered from api.github.com, so the `digest` it returned and the
//     checksum URLs it named are GitHub's own (callers record this as
//     `metadataFromOfficial`; metadata fetched through a mirror is NOT official, however
//     plausible it looks -- a mirror can forge a digest and a checksum URL as easily as
//     it can forge the bytes);
//   - a checksum URL derived from the package URL, when that URL itself comes from code
//     rather than from metadata (the built-in core bootstrap, `packageUrlFromCode`).
// A checksum document fetched from a mirror is never an official anchor: the mirror that
// served the bytes can serve a matching checksum for whatever it chose to serve.
//
// Verification therefore runs in two passes. `verifyDownloadedBytes` is the official pass:
// it only uses the sources above, fetched exactly as named (never through a mirror), and
// reports Verified, Mismatched, or Unverifiable. `verifyDownloadedBytesViaMirror` is the
// degraded pass for when GitHub itself is unreachable -- the exact situation the mirrors
// exist for: it tries the same sources through the mirrors as well and reports
// MirrorVerified when one matches. MirrorVerified means the bytes are self-consistent
// with what the mirror said, NOT that GitHub vouches for them; a compromised mirror can
// forge both, and callers must say so loudly instead of installing silently. A mismatch
// fails closed in both passes, and when no checksum exists anywhere the result is
// Unverifiable, never Verified.
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
    // Verified against a checksum that only a mirror vouches for (see the trust model
    // above). Installs proceed only where the caller accepts the degraded tradeoff, and
    // the caller must warn loudly.
    MirrorVerified,
    Mismatched,
    Unverifiable,
};

struct VerificationOutcome {
    Verification status = Verification::Unverifiable;
    // Lowercase hex. Set for Verified, MirrorVerified and Mismatched; empty for
    // Unverifiable.
    QString expectedHex;
    QString actualHex;
};

// Everything one verification needs. `fetchChecksum` is (url, content) ->
// OperationResult, the caller's own download path.
struct VerificationRequest {
    QByteArray packageBytes;
    QString assetName;
    QUrl packageDownloadUrl;
    QString apiDigest;
    std::optional<NamedAsset> checksumAsset;
    // True when the release metadata (and therefore the API digest and any checksum URL
    // the metadata names) arrived over the official GitHub channel. False is the safe
    // default: a digest from unknown provenance is treated as mirror-supplied.
    bool metadataFromOfficial = false;
    // True when the package URL itself comes from code, not from release metadata (the
    // built-in core bootstrap). Derived checksum URLs are then anchored to the URL's own
    // host instead of to metadata that does not exist.
    bool packageUrlFromCode = false;
};

using FetchChecksum =
    std::function<OperationResult(const QUrl& url, QByteArray* content)>;

// The official pass. Verifies `request.packageBytes` for `request.assetName`, in order:
//   1. `apiDigest`, when the metadata that carried it came from the official channel;
//   2. `checksumAsset`, when the metadata that named it came from the official channel
//      and its URL is a GitHub origin;
//   3. the checksum URLs derived from `packageDownloadUrl`, when their host is a GitHub
//      origin and either the metadata was official or the package URL comes from code.
// Checksum documents are fetched exactly as named, never through a mirror: a mirror
// must not get a chance to substitute the document the anchor is read from.
// Returns Mismatched (never Verified) when a hash was established and the bytes differ,
// and Unverifiable when no official hash could be established from any source.
VerificationOutcome verifyDownloadedBytes(
    const VerificationRequest& request,
    const FetchChecksum& fetchChecksum);

// The degraded pass, for when the official channel is unreachable and the mirrors are
// all that answer. Same sources as the official pass, but each checksum URL is also
// tried through the mirror candidates, and the API digest is used as a mirror-supplied
// value: a match here proves the bytes agree with the mirror, not with GitHub. Returns
// MirrorVerified on a match, Mismatched when a hash was established and the bytes
// differ, and Unverifiable when no hash could be established from any source.
VerificationOutcome verifyDownloadedBytesViaMirror(
    const VerificationRequest& request,
    const FetchChecksum& fetchChecksum);

} // namespace ReleaseChecksum
