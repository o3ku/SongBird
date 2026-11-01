#pragma once

// The stored interval for scheduled subscription refresh, clamped. Shared by the settings page
// (the spin box's range and the value it reads back) and the scheduler's due computation, which
// is why it lives in common/ -- ui/ may not include app/, and both need the same two numbers or
// the spin box could store an hour count the scheduler would silently read differently.
namespace SubscriptionAutoUpdateInterval {

// A fat-fingered 0 (or a hand-edited config file) must not become a request every tick, and a
// ceiling keeps the number honest even when it is typed rather than spun.
constexpr int kMinimumHours = 1;
constexpr int kMaximumHours = 24 * 7;

constexpr int normalizedHours(int hours)
{
    return hours < kMinimumHours
        ? kMinimumHours
        : (hours > kMaximumHours ? kMaximumHours : hours);
}

} // namespace SubscriptionAutoUpdateInterval
