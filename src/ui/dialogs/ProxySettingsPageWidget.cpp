#include "ui/dialogs/ProxySettingsPageWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

#include "ui/theme/AppTheme.h"

ProxySettingsPageWidget::ProxySettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ProxySettingsPageWidget::setConfig(const Config& config)
{
    systemProxyExceptionsEdit_->setText(config.systemProxyExceptions);
    systemProxyAdvancedProtocolCombo_->setCurrentText(config.systemProxyAdvancedProtocol);
    pacUrlEdit_->setText(config.pacUrl);
}

void ProxySettingsPageWidget::applyToConfig(Config& config) const
{
    config.systemProxyExceptions = systemProxyExceptionsEdit_->text().trimmed();
    config.systemProxyAdvancedProtocol = systemProxyAdvancedProtocolCombo_->currentText().trimmed();
    config.pacUrl = pacUrlEdit_->text().trimmed();
}

void ProxySettingsPageWidget::setupUi()
{
    auto* proxyLayout = new QFormLayout(this);

    systemProxyExceptionsEdit_ = new QLineEdit(this);
    systemProxyExceptionsEdit_->setObjectName(QStringLiteral("settingsSystemProxyExceptionsEdit"));
    systemProxyExceptionsEdit_->setPlaceholderText(QStringLiteral("localhost;127.*;192.168.*"));

    systemProxyAdvancedProtocolCombo_ = new QComboBox(this);
    systemProxyAdvancedProtocolCombo_->setObjectName(QStringLiteral("settingsSystemProxyAdvancedProtocolCombo"));
    systemProxyAdvancedProtocolCombo_->setEditable(true);
    systemProxyAdvancedProtocolCombo_->addItems({
        QString(),
        QStringLiteral("{ip}:{http_port}"),
        QStringLiteral("socks={ip}:{socks_port}"),
        QStringLiteral("http={ip}:{http_port};https={ip}:{http_port};ftp={ip}:{http_port};socks={ip}:{socks_port}"),
        QStringLiteral("http=http://{ip}:{http_port};https=http://{ip}:{http_port}")
    });

    pacUrlEdit_ = new QLineEdit(this);
    pacUrlEdit_->setObjectName(QStringLiteral("settingsPacUrlEdit"));
    pacUrlEdit_->setPlaceholderText(
        tr("Leave empty to use built-in PAC server (http://127.0.0.1:10870/pac)"));
    pacUrlEdit_->setVisible(false);

    AppTheme::applyCompactFont({
        systemProxyExceptionsEdit_,
        systemProxyAdvancedProtocolCombo_,
        pacUrlEdit_});

    proxyLayout->addRow(tr("System Proxy Exceptions"), systemProxyExceptionsEdit_);
    proxyLayout->addRow(tr("Advanced Proxy Protocol"), systemProxyAdvancedProtocolCombo_);
}
