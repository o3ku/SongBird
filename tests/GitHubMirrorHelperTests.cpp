#include <QtTest>

#include <QUrl>

#include "common/GitHubMirrorHelper.h"

namespace {

// A download URL on the official channel, as a release asset would carry it.
QUrl releaseUrl(const QString& name)
{
    return QUrl(QStringLiteral("https://github.com/XTLS/Xray-core/releases/download/v26.3.27/") + name);
}

} // namespace

class GitHubMirrorHelperTests : public QObject {
    Q_OBJECT

private slots:
    void mirrorPrefixesAreExactlyTheThreeIntentionalMirrors();
    void mirrorCandidatesTryMirrorsFirstThenOfficial();
    void officialFirstCandidatesTryOfficialFirstThenMirrors();
    void nonGitHubUrlsAreNotRewritten();
    void isGitHubOriginUrlRecognisesOrigins();
};

void GitHubMirrorHelperTests::mirrorPrefixesAreExactlyTheThreeIntentionalMirrors()
{
    // The three intentional mirrors, in the order they are tried, from the base commit.
    // Adding a host here is a deliberate allowlist decision, not a string nobody reads
    // twice -- and the mirror allowlist guard fails the build on a host it does not name.
    const QList<QUrl> candidates = buildGitHubMirrorCandidateUrls(releaseUrl(QStringLiteral("x.zip")));
    QCOMPARE(candidates.size(), 4);
    QCOMPARE(candidates.at(0).toString(),
             QStringLiteral("https://ghfast.top/https://github.com/XTLS/Xray-core/releases/download/v26.3.27/x.zip"));
    QCOMPARE(candidates.at(1).toString(),
             QStringLiteral("https://mirror.ghproxy.com/https://github.com/XTLS/Xray-core/releases/download/v26.3.27/x.zip"));
    QCOMPARE(candidates.at(2).toString(),
             QStringLiteral("https://ghproxy.net/https://github.com/XTLS/Xray-core/releases/download/v26.3.27/x.zip"));
    QCOMPARE(candidates.at(3), releaseUrl(QStringLiteral("x.zip")));
}

void GitHubMirrorHelperTests::mirrorCandidatesTryMirrorsFirstThenOfficial()
{
    // Mirrors exist for regions where GitHub is unreachable, so a package download tries
    // them first and only falls back to the official URL last.
    const QList<QUrl> candidates = buildGitHubMirrorCandidateUrls(releaseUrl(QStringLiteral("x.zip")));
    QCOMPARE(candidates.first().host(), QStringLiteral("ghfast.top"));
    QCOMPARE(candidates.last().host(), QStringLiteral("github.com"));
}

void GitHubMirrorHelperTests::officialFirstCandidatesTryOfficialFirstThenMirrors()
{
    // Metadata decides whether the checksum anchor is trustworthy, so it is fetched
    // official-first: one attempt on the official channel, then the mirrors.
    const QList<QUrl> candidates =
        buildGitHubOfficialFirstCandidateUrls(QUrl(QStringLiteral("https://api.github.com/repos/o3ku/songbird/releases/latest")));
    QCOMPARE(candidates.size(), 4);
    QCOMPARE(candidates.first().host(), QStringLiteral("api.github.com"));
    QCOMPARE(candidates.at(1).host(), QStringLiteral("ghfast.top"));
    QCOMPARE(candidates.at(2).host(), QStringLiteral("mirror.ghproxy.com"));
    QCOMPARE(candidates.at(3).host(), QStringLiteral("ghproxy.net"));
}

void GitHubMirrorHelperTests::nonGitHubUrlsAreNotRewritten()
{
    // A URL that is not a GitHub origin is not a download this helper routes: both
    // builders pass it through unchanged.
    const QUrl url(QStringLiteral("https://example.invalid/x.zip"));
    QCOMPARE(buildGitHubMirrorCandidateUrls(url), QList<QUrl>{url});
    QCOMPARE(buildGitHubOfficialFirstCandidateUrls(url), QList<QUrl>{url});
}

void GitHubMirrorHelperTests::isGitHubOriginUrlRecognisesOrigins()
{
    QVERIFY(isGitHubOriginUrl(QUrl(QStringLiteral("https://github.com/o3ku/songbird"))));
    QVERIFY(isGitHubOriginUrl(QUrl(QStringLiteral("https://api.github.com/repos/o3ku/songbird"))));
    QVERIFY(isGitHubOriginUrl(QUrl(QStringLiteral("https://objects.githubusercontent.com/x"))));
    QVERIFY(isGitHubOriginUrl(QUrl(QStringLiteral("https://raw.githubusercontent.com/o3ku/songbird/main/x"))));
    QVERIFY(!isGitHubOriginUrl(QUrl(QStringLiteral("https://ghfast.top/https://github.com/x"))));
    QVERIFY(!isGitHubOriginUrl(QUrl(QStringLiteral("https://mirror.ghproxy.com/https://github.com/x"))));
    QVERIFY(!isGitHubOriginUrl(QUrl(QStringLiteral("https://example.invalid/x.zip"))));
    QVERIFY(!isGitHubOriginUrl(QUrl(QStringLiteral("https://notgithub.com/x"))));
}

QTEST_MAIN(GitHubMirrorHelperTests)

#include "GitHubMirrorHelperTests.moc"
