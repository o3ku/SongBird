#pragma once

struct CoreTypeItem {
    int configType = 0; // EConfigType
    int coreType = 0;   // ECoreType

    bool operator==(const CoreTypeItem& other) const = default;
    bool operator!=(const CoreTypeItem& other) const = default;
};
