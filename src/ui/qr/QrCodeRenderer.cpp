#include "ui/qr/QrCodeRenderer.h"

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QtGlobal>

#include "third_party/qrcodegen/qrcodegen.hpp"

QPixmap QrCodeRenderer::render(const QString& text, int size)
{
    if (text.trimmed().isEmpty() || size <= 0) {
        return {};
    }

    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
        text.toUtf8().constData(),
        qrcodegen::QrCode::Ecc::MEDIUM);

    const int qrSize = qr.getSize();

    QImage image(qrSize, qrSize, QImage::Format_ARGB32);
    image.fill(Qt::white);

    for (int y = 0; y < qrSize; ++y) {
        for (int x = 0; x < qrSize; ++x) {
            if (qr.getModule(x, y)) {
                image.setPixelColor(x, y, Qt::black);
            }
        }
    }

    const qreal dpr = qMax(
        qreal(1.0),
        QGuiApplication::instance() ? qApp->devicePixelRatio() : qreal(1.0));
    const int physicalSize = qMax(1, qRound(size * dpr));

    QPixmap pixmap = QPixmap::fromImage(image.scaled(
        physicalSize,
        physicalSize,
        Qt::KeepAspectRatio,
        Qt::FastTransformation));
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}
