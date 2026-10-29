#pragma once
#include <array>
#include <cmath>

namespace platform::adapter
{
    // Column-major local layer transform, applied before the existing GUI item
    // centering/rotation. Matches 26.1 LayerRenderState.applyTransform order.
    inline bool apply_item_local_transform(std::array<float, 3>& position,
        const std::array<float, 16>& matrix)
    {
        for (const auto value : matrix) if (!std::isfinite(value)) return false;
        if (matrix[3] != 0.f || matrix[7] != 0.f || matrix[11] != 0.f || matrix[15] != 1.f) return false;
        std::array<float, 3> result{};
        for (int axis = 0; axis < 3; ++axis)
        {
            result[axis] = matrix[axis] * position[0] + matrix[4 + axis] * position[1]
                + matrix[8 + axis] * position[2] + matrix[12 + axis];
            if (!std::isfinite(result[axis])) return false;
        }
        position = result;
        return true;
    }
}
