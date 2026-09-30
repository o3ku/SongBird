#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <QString>
#include <QStringList>

// Ordered startup steps with abort-on-failure semantics.
//
// AppBootstrap::run() used to inline its startup tail (orphan-core cleanup,
// tray initialization, config reload, system-proxy adoption, window
// presentation, update timers) directly in the composition root, so the order
// was implicit in the control flow and nothing could assert on it. Declaring
// the sequence as data keeps run() a pure assembly step and makes both the
// ordering and the abort rule testable without a live config or a core process.
//
// Two kinds of entry exist because they read differently at the call site:
// addStep() is unconditional, addGate() can stop the sequence.
class StartupSequencer {
public:
    // Unconditional step: always continues the sequence.
    using Step = std::function<void()>;
    // Guarded step: returning false aborts the sequence.
    using Gate = std::function<bool()>;

    void addStep(QString name, Step step)
    {
        addEntry(std::move(name), [step = std::move(step)]() {
            step();
            return true;
        });
    }

    void addGate(QString name, Gate gate)
    {
        addEntry(std::move(name), std::move(gate));
    }

    // Runs every entry in declaration order and stops at the first one that
    // returns false. Returns true only when all of them ran to completion.
    bool runAll()
    {
        executedStepCount_ = 0;
        failedStepName_.clear();

        for (const Entry& entry : steps_) {
            ++executedStepCount_;
            if (!entry.gate()) {
                failedStepName_ = entry.name;
                return false;
            }
        }

        return true;
    }

    int stepCount() const
    {
        return static_cast<int>(steps_.size());
    }

    // Number of entries entered by the most recent runAll(). Equals stepCount()
    // when the sequence completed.
    int executedStepCount() const
    {
        return executedStepCount_;
    }

    // Name of the entry that aborted the most recent runAll(), or an empty
    // string when the sequence completed.
    QString failedStepName() const
    {
        return failedStepName_;
    }

    QStringList stepNames() const
    {
        QStringList names;
        names.reserve(static_cast<int>(steps_.size()));
        for (const Entry& entry : steps_) {
            names.append(entry.name);
        }
        return names;
    }

private:
    struct Entry {
        QString name;
        Gate gate;
    };

    void addEntry(QString name, Gate gate)
    {
        steps_.push_back(Entry{std::move(name), std::move(gate)});
    }

    std::vector<Entry> steps_;
    int executedStepCount_ = 0;
    QString failedStepName_;
};
