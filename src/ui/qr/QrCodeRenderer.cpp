#include "ui/qr/QrCodeRenderer.h"

#include <QColor>
#include <QImage>
#include <QPainter>

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

    return QPixmap::fromImage(image.scaled(
        size,
        size,
        Qt::KeepAspectRatio,
        Qt::FastTransformation));
}
