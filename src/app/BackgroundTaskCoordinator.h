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

    explicit BackgroundTaskCoordinator(QObject* parent = nullptr);

    void setBlockingPredicate(std::function<bool()> predicate);

    bool tryBegin(Kind kind);
    void finish(Kind kind);

    Kind active() const;
    bool isActive() const;
    bool isKindActive(Kind kind) const;

    void resetSpeedTestProgress();
    void setSpeedTestTotalCount(int total);
    void recordSpeedTestResult(const QString& serverName);

    void syncState();

signals:
    void runningChanged(bool running);
    void descriptionChanged(const QString& description);
    void blockedByCoreStartup();

private:
    QString describe(Kind kind) const;
    void emitCurrent();

    Kind activeKind_ = Kind::None;
    int speedTestTotalCount_ = 0;
    int speedTestCompletedCount_ = 0;
    QString speedTestProgressServerName_;
    std::function<bool()> blockingPredicate_;
};
