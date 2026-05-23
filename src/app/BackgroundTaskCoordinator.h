#pragma once

#include <functional>

#include <QObject>
#include <QString>

class BackgroundTaskCoordinator : public QObject {
    Q_OBJECT

public:
    enum class Kind {
        None,
        SpeedTest,
        SubscriptionUpdate,
        ProxyAvailabilityCheck,
        CoreUpdate,
        GeoUpdate
    };

    struct Token {
        Kind kind = Kind::None;
        quint64 generation = 0;

        bool isValid() const;
    };

    explicit BackgroundTaskCoordinator(QObject* parent = nullptr);

    void setBlockingPredicate(std::function<bool()> predicate);

    Token tryBeginUserTask(Kind kind);
    Token tryBeginStartupTask(Kind kind);
    bool finish(const Token& token);
    void cancelActive();

    Kind active() const;
    bool isActive() const;
    bool isKindActive(Kind kind) const;
    bool isCurrent(const Token& token) const;

    void resetSpeedTestProgress();
    void setSpeedTestTotalCount(int total);
    void recordSpeedTestResult(const QString& serverName);

    void syncState();

signals:
    void runningChanged(bool running);
    void descriptionChanged(const QString& description);
    void blockedByCoreStartup();

private:
    enum class StartScope {
        UserTask,
        StartupTask
    };

    Token tryBegin(Kind kind, StartScope scope);
    QString describe(Kind kind) const;
    void emitCurrent();

    Kind activeKind_ = Kind::None;
    quint64 generation_ = 0;
    quint64 activeGeneration_ = 0;
    int speedTestTotalCount_ = 0;
    int speedTestCompletedCount_ = 0;
    QString speedTestProgressServerName_;
    std::function<bool()> blockingPredicate_;
};
