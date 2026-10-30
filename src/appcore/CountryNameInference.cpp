#include "appcore/CountryNameInference.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>

namespace {

QString flagCountryCode(const QString& name)
{
    const QVector<uint> codePoints = name.toUcs4();
    for (int i = 0; i + 1 < codePoints.size(); ++i) {
        const uint first = codePoints.at(i);
        const uint second = codePoints.at(i + 1);
        if (first < 0x1F1E6u || first > 0x1F1FFu || second < 0x1F1E6u || second > 0x1F1FFu) {
            continue;
        }
        const QChar firstLetter(static_cast<ushort>(QLatin1Char('A').unicode() + first - 0x1F1E6u));
        const QChar secondLetter(static_cast<ushort>(QLatin1Char('A').unicode() + second - 0x1F1E6u));
        return QString(firstLetter) + QString(secondLetter);
    }
    return {};
}

QString aliasCountryCode(const QString& name)
{
    const QString normalized = name.toUpper();
    static const QList<QPair<QString, QString>> aliases{
        {QStringLiteral("香港"), QStringLiteral("HK")},
        {QStringLiteral("HONG KONG"), QStringLiteral("HK")},
        {QStringLiteral("HONGKONG"), QStringLiteral("HK")},
        {QStringLiteral("台湾"), QStringLiteral("TW")},
        {QStringLiteral("TAIWAN"), QStringLiteral("TW")},
        {QStringLiteral("美国"), QStringLiteral("US")},
        {QStringLiteral("UNITED STATES"), QStringLiteral("US")},
        {QStringLiteral("AMERICA"), QStringLiteral("US")},
        {QStringLiteral("日本"), QStringLiteral("JP")},
        {QStringLiteral("JAPAN"), QStringLiteral("JP")},
        {QStringLiteral("新加坡"), QStringLiteral("SG")},
        {QStringLiteral("SINGAPORE"), QStringLiteral("SG")},
        {QStringLiteral("韩国"), QStringLiteral("KR")},
        {QStringLiteral("KOREA"), QStringLiteral("KR")},
        // City aliases for the countries the location view groups by: subscription names are
        // frequently city-only ("首尔 01", "Tokyo 02"), and a city that uniquely names its country
        // is as good a mark as the country word itself. Korean and Japanese city words are also
        // the ones the token pass cannot catch (Hangul and Han scripts carry no roman letters).
        {QStringLiteral("首尔"), QStringLiteral("KR")},
        {QStringLiteral("SEOUL"), QStringLiteral("KR")},
        {QStringLiteral("东京"), QStringLiteral("JP")},
        {QStringLiteral("大阪"), QStringLiteral("JP")},
        {QStringLiteral("英国"), QStringLiteral("GB")},
        {QStringLiteral("UNITED KINGDOM"), QStringLiteral("GB")},
        {QStringLiteral("GERMANY"), QStringLiteral("DE")},
        {QStringLiteral("德国"), QStringLiteral("DE")},
        {QStringLiteral("法国"), QStringLiteral("FR")},
        {QStringLiteral("FRANCE"), QStringLiteral("FR")},
        {QStringLiteral("加拿大"), QStringLiteral("CA")},
        {QStringLiteral("CANADA"), QStringLiteral("CA")},
        {QStringLiteral("澳大利亚"), QStringLiteral("AU")},
        {QStringLiteral("AUSTRALIA"), QStringLiteral("AU")},
        {QStringLiteral("荷兰"), QStringLiteral("NL")},
        {QStringLiteral("NETHERLANDS"), QStringLiteral("NL")},
        {QStringLiteral("俄罗斯"), QStringLiteral("RU")},
        {QStringLiteral("RUSSIA"), QStringLiteral("RU")},
        {QStringLiteral("土耳其"), QStringLiteral("TR")},
        {QStringLiteral("TURKEY"), QStringLiteral("TR")},
        {QStringLiteral("印度"), QStringLiteral("IN")},
        {QStringLiteral("INDIA"), QStringLiteral("IN")},
        {QStringLiteral("泰国"), QStringLiteral("TH")},
        {QStringLiteral("THAILAND"), QStringLiteral("TH")},
        {QStringLiteral("越南"), QStringLiteral("VN")},
        {QStringLiteral("VIETNAM"), QStringLiteral("VN")},
        {QStringLiteral("马来西亚"), QStringLiteral("MY")},
        {QStringLiteral("MALAYSIA"), QStringLiteral("MY")},
        {QStringLiteral("菲律宾"), QStringLiteral("PH")},
        {QStringLiteral("PHILIPPINES"), QStringLiteral("PH")},
        {QStringLiteral("印尼"), QStringLiteral("ID")},
        {QStringLiteral("印度尼西亚"), QStringLiteral("ID")},
        {QStringLiteral("INDONESIA"), QStringLiteral("ID")},
    };

    for (const auto& alias : aliases) {
        if (normalized.contains(alias.first)) {
            return alias.second;
        }
    }
    return {};
}

QString tokenCountryCode(const QString& name)
{
    static const QHash<QString, QString> codes{
        {QStringLiteral("AU"), QStringLiteral("AU")},
        {QStringLiteral("DE"), QStringLiteral("DE")},
        {QStringLiteral("GB"), QStringLiteral("GB")},
        {QStringLiteral("HK"), QStringLiteral("HK")},
        {QStringLiteral("ID"), QStringLiteral("ID")},
        {QStringLiteral("JP"), QStringLiteral("JP")},
        {QStringLiteral("KR"), QStringLiteral("KR")},
        {QStringLiteral("MY"), QStringLiteral("MY")},
        {QStringLiteral("SG"), QStringLiteral("SG")},
        {QStringLiteral("TW"), QStringLiteral("TW")},
        {QStringLiteral("UK"), QStringLiteral("GB")},
        {QStringLiteral("US"), QStringLiteral("US")},
        {QStringLiteral("USA"), QStringLiteral("US")},
    };

    static const QRegularExpression tokenPattern(QStringLiteral("(^|[^A-Za-z])([A-Za-z]{2,3})(?=$|[^A-Za-z])"));
    QRegularExpressionMatchIterator it = tokenPattern.globalMatch(name);
    while (it.hasNext()) {
        const QString token = it.next().captured(2).toUpper();
        const QString countryCode = codes.value(token);
        if (!countryCode.isEmpty()) {
            return countryCode;
        }
    }
    return {};
}

} // namespace

namespace CountryNameInference {

QString unknownCountryCode()
{
    return QStringLiteral("UNKNOWN");
}

QString inferCountryCodeFromNodeName(const QString& name)
{
    const QString flagCode = flagCountryCode(name);
    if (!flagCode.isEmpty()) {
        return flagCode;
    }

    const QString aliasCode = aliasCountryCode(name);
    if (!aliasCode.isEmpty()) {
        return aliasCode;
    }

    const QString tokenCode = tokenCountryCode(name);
    return tokenCode.isEmpty() ? unknownCountryCode() : tokenCode;
}

} // namespace CountryNameInference
