#pragma once

#include <QPixmap>
#include <QString>

class QrCodeRenderer {
public:
    static QPixmap render(const QString& text, int size = 240);
};
