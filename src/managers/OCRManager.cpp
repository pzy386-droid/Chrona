#include "managers/OCRManager.h"

#include <QFileInfo>
#include <QObject>
#include <QUrl>

QVariantMap OCRManager::previewImage(const QString& fileUrlOrPath) const
{
    const QUrl url(fileUrlOrPath);
    const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
    const QString name = QFileInfo(path).fileName();

    return {
        {QStringLiteral("fileName"), name},
        {QStringLiteral("recognizedText"), QObject::tr("下周三交数据库实验报告，预计 2 小时")},
        {QStringLiteral("message"), QObject::tr("当前为 OCR 预留接口，已生成可编辑识别结果")}
    };
}
