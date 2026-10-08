#pragma once

#include <algorithm>

namespace GameplayRules
{
/// Consume one medkit only when it can restore at least one point of health.
inline bool TryUseMedPack(int& hp, int maxHp, int& medPacks, int restoreAmount = 40)
{
    if (medPacks <= 0 || hp >= maxHp || restoreAmount <= 0) return false;
    hp = std::min(maxHp, hp + restoreAmount);
    --medPacks;
    return true;
}
}
