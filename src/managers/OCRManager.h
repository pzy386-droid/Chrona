#pragma once

#include <QVariantMap>

class OCRManager {
public:
    QVariantMap previewImage(const QString& fileUrlOrPath) const;
};
