#pragma once

#include <QString>

#include <utility>

struct OperationResult {
    bool success = false;
    QString message;
    bool requiresRestart = false;
    bool cancelled = false;

    static OperationResult ok(QString message = {}, bool requiresRestart = false)
    {
        return {true, std::move(message), requiresRestart, false};
    }

    static OperationResult fail(QString message)
    {
        return {false, std::move(message), false, false};
    }

    static OperationResult cancel(QString message)
    {
        return {false, std::move(message), false, true};
    }
};
