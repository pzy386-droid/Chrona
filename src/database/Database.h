#pragma once

#include <QSqlDatabase>
#include <QString>

class Database {
public:
    bool open();
    QSqlDatabase connection() const;
    QString lastError() const;

private:
    QString m_connectionName = QStringLiteral("chrona");
    QString m_lastError;
};
