#pragma once

#include <QString>
#include <QStringList>

inline QString fallbackUserAgent()
{
    // Karing's default client UA is its kUserAgentList[0]: Karing sends
    // mihomo/1.19.28 itself when no compatible UA is selected
    // (KaringX/karing, setting_manager.dart kUserAgentList).
    return QStringLiteral("mihomo/1.19.28");
}

struct SubscriptionUserAgentPreset
{
    const char* label;
    const char* userAgent;
};

inline const SubscriptionUserAgentPreset* subscriptionUserAgentPresets(int* count)
{
    static const SubscriptionUserAgentPreset kPresets[] = {
        // SongBird's own presets; "nekobox" maps to fallbackUserAgent().
        {"nekobox", ""},
        {"clash verge rev", "clash-verge/v2.4"},
        {"v2rayn", "v2rayN/7.10.4"},
        {"shadowrocket", "Shadowrocket/2.2.56"},
        {"surge", "Surge/5.0"},
        {"sing-box", "sing-box/1.11.0"},
        {"hiddify", "Hiddify/2.5.7"},
        {"loon", "Loon/3.2.4"},
        // Entries borrowed from Karing's default user-agent list
        // (KaringX/karing, setting_manager.dart kUserAgentList,
        // kCoreVersion 1.13.0). Karing's own list holds no 'karing/...'
        // client UA at all: its first entry, mihomo/1.19.28, is what it
        // actually sends when no compatible UA is selected, and 'sing-box'
        // resolves to its live core version ('sing-box 1.13.0') while
        // SongBird keeps a fixed one.
        {"mihomo", "mihomo/1.19.28"},
        {"clash verge", "clash-verge"},
        {"flclash", "FLClash"},
        {"mihomo party", "mihomo.party/v2.0.0 (clash.meta)"},
        {"clash meta", "ClashMeta"},
        {"v2ray", "v2ray"},
        {"nekobox android", "NekoBox/Android/1.4.1 (Prefer ClashMeta Format)"},
        {"hiddifynext", "HiddifyNext"},
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
    if (trimmed == QStringLiteral("Hiddify")) {
        return QStringLiteral("Hiddify/2.5.7");
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
