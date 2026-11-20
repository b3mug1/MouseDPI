#pragma once

#include <memory>
#include "core/types.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"

namespace mc::app::use_cases {

class SetPollingRateUseCase {
public:
    SetPollingRateUseCase() = default;

    core::Result<void> execute(core::domain::IMouseAdapter* adapter, int rate_hz);
};

} // namespace mc::app::use_cases
