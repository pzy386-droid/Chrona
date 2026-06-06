#pragma once

#include <QSqlDatabase>
#include <QString>

class BackupService {
public:
    explicit BackupService(QSqlDatabase db);

    bool exportToFile(const QString& filePath) const;
    bool importFromFile(const QString& filePath) const;
    QString lastError() const;

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};
