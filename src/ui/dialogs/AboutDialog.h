#pragma once

#include <QDialog>

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

private:
    void setupUi();

    QSvgWidget* logoLabel_ = nullptr;
    QLabel* repoUrlValueLabel_ = nullptr;
    QLabel* versionValueLabel_ = nullptr;
    QLabel* releaseDateValueLabel_ = nullptr;
    QLabel* configPathValueLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
