#include "platform/windows/WindowsThemePreferenceService.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <windows.h>

#include "ui/theme/AppThemePalette.h"

namespace {

constexpr auto kPersonalizeRegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr auto kAppsUseLightThemeValue = "AppsUseLightTheme";

} // namespace

// Watches the WM_SETTINGCHANGE broadcast. Windows sends it with lParam pointing at
// the string "ImmersiveColorSet" when the user switches between light and dark, which
// is the signal the Settings app itself reacts to. Filtering on that string keeps the
// theme from being re-resolved for every unrelated setting change.
class WindowsThemePreferenceService::NativeFilter final : public QAbstractNativeEventFilter {
public:
    explicit NativeFilter(WindowsThemePreferenceService* owner)
        : owner_(owner)
    {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override
    {
        Q_UNUSED(eventType)
        Q_UNUSED(result)

        auto* msg = static_cast<MSG*>(message);
        if (msg == nullptr || msg->message != WM_SETTINGCHANGE) {
            return false;
        }

        if (msg->lParam != 0) {
            const QString changedArea = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(msg->lParam));
            if (changedArea.compare(QStringLiteral("ImmersiveColorSet"), Qt::CaseInsensitive) != 0) {
                return false;
            }
        }

        if (owner_ != nullptr) {
            emit owner_->preferredThemeChanged();
        }

        return false;
    }

private:
    WindowsThemePreferenceService* owner_ = nullptr;
};

WindowsThemePreferenceService::WindowsThemePreferenceService(QObject* parent)
    : QObject(parent)
{
}

WindowsThemePreferenceService::~WindowsThemePreferenceService()
{
    if (nativeFilter_ != nullptr && qApp != nullptr) {
        qApp->removeNativeEventFilter(nativeFilter_);
    }
    delete nativeFilter_;
}

QString WindowsThemePreferenceService::preferredThemeName() const
{
    QSettings settings(QString::fromUtf8(kPersonalizeRegistryPath), QSettings::NativeFormat);
    const QVariant value = settings.value(QString::fromUtf8(kAppsUseLightThemeValue));
    if (!value.isValid()) {
        // Absent means "never chosen", which Windows treats as light.
        return QString::fromLatin1(AppThemeInternal::kThemeLight);
    }

    const int appsUseLightTheme = value.toInt();
    return appsUseLightTheme == 0
        ? QString::fromLatin1(AppThemeInternal::kThemeDark)
        : QString::fromLatin1(AppThemeInternal::kThemeLight);
}

bool WindowsThemePreferenceService::startWatching()
{
    if (nativeFilter_ != nullptr || qApp == nullptr) {
        return nativeFilter_ != nullptr;
    }

    nativeFilter_ = new NativeFilter(this);
    qApp->installNativeEventFilter(nativeFilter_);
    return true;
}
