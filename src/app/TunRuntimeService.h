#pragma once

#include "common/OperationResult.h"

class TunRuntimeService
{
public:
    OperationResult removeStaleAdapterIfPresent() const;

private:
    bool isAdapterPresent() const;
};
