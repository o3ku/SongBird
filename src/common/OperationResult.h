#pragma once

#include <QList>
#include <QString>

#include <utility>

struct OperationResult {
    // One subject's failure, for a result that covers several. `id` is the caller's own key for
    // the subject (a subscription id), so a caller that needs "what failed and why" per subject
    // does not have to parse the translated message back into one: the batch line is prose for
    // the log panel, while the attribution is a fact the producer already knows.
    struct SubjectFailure {
        QString id;
        QString reason;
    };

    bool success = false;
    QString message;
    bool requiresRestart = false;
    bool cancelled = false;
    QList<SubjectFailure> subjectFailures;

    static OperationResult ok(QString message = {}, bool requiresRestart = false)
    {
        return {true, std::move(message), requiresRestart, false, {}};
    }

    static OperationResult fail(QString message)
    {
        return {false, std::move(message), false, false, {}};
    }

    static OperationResult cancel(QString message)
    {
        return {false, std::move(message), false, true, {}};
    }

    OperationResult& withSubjectFailure(QString id, QString reason)
    {
        subjectFailures.append({std::move(id), std::move(reason)});
        return *this;
    }
};
