#pragma once

#include <QSqlDatabase>
#include <QString>

class Migrations {
public:
    static bool run(QSqlDatabase db);
    static bool seed(QSqlDatabase db);
    static int currentVersion();
    static QString lastError();
};
