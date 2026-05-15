#pragma once

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace DialogUtils {

inline void localizeStandardDialogButtonBox(QDialogButtonBox* buttonBox)
{
    if (buttonBox == nullptr) {
        return;
    }

    const bool isZhCN = QLocale().name() == QStringLiteral("zh_CN");
    if (!isZhCN) {
        return;
    }

    if (auto* btn = buttonBox->button(QDialogButtonBox::Ok)) {
        btn->setText(QStringLiteral("确定"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Cancel)) {
        btn->setText(QStringLiteral("取消"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Yes)) {
        btn->setText(QStringLiteral("是"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::No)) {
        btn->setText(QStringLiteral("否"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Open)) {
        btn->setText(QStringLiteral("打开"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Save)) {
        btn->setText(QStringLiteral("保存"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Close)) {
        btn->setText(QStringLiteral("关闭"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Apply)) {
        btn->setText(QStringLiteral("应用"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Reset)) {
        btn->setText(QStringLiteral("重置"));
    }
    if (auto* btn = buttonBox->button(QDialogButtonBox::Help)) {
        btn->setText(QStringLiteral("帮助"));
    }
}

inline QMessageBox::StandardButton askYesNoQuestion(
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButton defaultButton = QMessageBox::No)
{
    QMessageBox messageBox(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    auto* yesButton = messageBox.addButton(QMessageBox::Yes);
    auto* noButton = messageBox.addButton(QMessageBox::No);
    if (QLocale().name() == QStringLiteral("zh_CN")) {
        yesButton->setText(QStringLiteral("是"));
        noButton->setText(QStringLiteral("否"));
    }
    messageBox.setDefaultButton(defaultButton == QMessageBox::Yes ? yesButton : noButton);
    messageBox.exec();
    return messageBox.standardButton(messageBox.clickedButton());
}

inline void showInformation(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox messageBox(QMessageBox::Information, title, text, QMessageBox::NoButton, parent);
    auto* okButton = messageBox.addButton(QMessageBox::Ok);
    if (QLocale().name() == QStringLiteral("zh_CN")) {
        okButton->setText(QStringLiteral("确定"));
    }
    messageBox.setDefaultButton(okButton);
    messageBox.exec();
}

inline void showWarning(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox messageBox(QMessageBox::Warning, title, text, QMessageBox::NoButton, parent);
    auto* okButton = messageBox.addButton(QMessageBox::Ok);
    if (QLocale().name() == QStringLiteral("zh_CN")) {
        okButton->setText(QStringLiteral("确定"));
    }
    messageBox.setDefaultButton(okButton);
    messageBox.exec();
}

inline void showCritical(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox messageBox(QMessageBox::Critical, title, text, QMessageBox::NoButton, parent);
    auto* okButton = messageBox.addButton(QMessageBox::Ok);
    if (QLocale().name() == QStringLiteral("zh_CN")) {
        okButton->setText(QStringLiteral("确定"));
    }
    messageBox.setDefaultButton(okButton);
    messageBox.exec();
}

inline QString getOpenFileName(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, QString(), filter);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    if (!initialPath.trimmed().isEmpty()) {
        dialog.selectFile(initialPath);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().constFirst();
}

inline QString getSaveFileName(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, QString(), filter);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    if (!initialPath.trimmed().isEmpty()) {
        dialog.selectFile(initialPath);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().constFirst();
}

} // namespace DialogUtils
