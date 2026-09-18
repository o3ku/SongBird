#pragma once

// Shared network timeouts for the update/subscription services so polling,
// metadata fetch, and large download budgets stay consistent across services.
namespace ServiceTimeouts {

// Metadata / subscription content fetches: short network round trips.
constexpr int kDefaultNetworkTimeoutMs = 30000;
// Large installer/archive downloads (app update, core update packages).
constexpr int kLargeDownloadTimeoutMs = 180000;
// Polling cadence for cancellation checks inside network wait loops.
constexpr int kCancellationPollIntervalMs = 100;

} // namespace ServiceTimeouts
