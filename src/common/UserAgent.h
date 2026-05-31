#pragma once

#include <QString>
#include <QStringList>

inline QString fallbackUserAgent()
{
    return QStringLiteral("nekobox/5.11.15 (Prefer ClashMeta Format)");
}

struct SubscriptionUserAgentPreset
{
    const char* label;
    const char* userAgent;
};

inline const SubscriptionUserAgentPreset* subscriptionUserAgentPresets(int* count)
{
    static const SubscriptionUserAgentPreset kPresets[] = {
        {"nekobox", ""},
        {"clash verge rev", "clash-verge/v2.4"},
        {"v2rayn", "v2rayN/7.10.4"},
        {"shadowrocket", "Shadowrocket/2.2.56"},
        {"surge", "Surge/5.0"},
        {"sing-box", "sing-box/1.11.0"},
        {"hiddify", "Hiddify/2.5.7"},
        {"loon", "Loon/3.2.4"},
    };

    if (count != nullptr) {
        *count = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
    }
    return kPresets;
}

inline QStringList subscriptionUserAgentPresetLabels()
{
    int count = 0;
    const SubscriptionUserAgentPreset* presets = subscriptionUserAgentPresets(&count);
    QStringList labels;
    labels.reserve(count);
    for (int i = 0; i < count; ++i) {
        labels.append(QString::fromLatin1(presets[i].label));
    }
    return labels;
}

inline QString resolveSubscriptionUserAgent(const QString& storedValue)
{
    const QString trimmed = storedValue.trimmed();
    if (trimmed.isEmpty()
        || trimmed == QStringLiteral("Nekobox")
        || trimmed == QStringLiteral("nekobox")) {
        return fallbackUserAgent();
    }
    if (trimmed == QStringLiteral("ClashVerge")) {
        return QStringLiteral("clash-verge/v2.4");
    }

    int count = 0;
    const SubscriptionUserAgentPreset* presets = subscriptionUserAgentPresets(&count);
    for (int i = 0; i < count; ++i) {
        if (trimmed == QString::fromLatin1(presets[i].label)) {
            const QString presetUserAgent = QString::fromLatin1(presets[i].userAgent);
            return presetUserAgent.isEmpty() ? fallbackUserAgent() : presetUserAgent;
        }
    }
    return trimmed;
}
