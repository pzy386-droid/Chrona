#pragma once

#include <QSqlDatabase>

class Migrations {
public:
    static bool run(QSqlDatabase db);
    static bool seed(QSqlDatabase db);
};
