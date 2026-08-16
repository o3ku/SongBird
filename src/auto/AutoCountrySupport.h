#pragma once

#include <QString>
#include <QStringList>

QStringList autoFixedCountryCodes();
bool isAutoFixedCountryCode(const QString& countryCode);
QString normalizeAutoCountryCode(const QString& value);
QString autoUnknownCountryCode();
QString autoCountryDisplayName(const QString& countryCode, const QString& countryName = {});
