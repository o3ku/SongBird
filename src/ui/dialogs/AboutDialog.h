#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QSvgWidget;
class QDialogButtonBox;

class AboutDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    void setRepoUrl(const QString& repoUrl);
    void setReleaseDate(const QString& releaseDate);
    void setVersion(const QString& version);
    void setConfigPath(const QString& configPath);
    void setProxyActive(bool active);

private:
    void setupUi();
    void refreshSubtitle();

    QSvgWidget* logoLabel_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    QLabel* repoUrlValueLabel_ = nullptr;
    QLabel* configPathValueLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
    QString version_;
    QString releaseDate_;
};
