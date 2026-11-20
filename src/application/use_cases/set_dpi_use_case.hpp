#pragma once

#include <memory>
#include "core/types.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"

namespace mc::app::use_cases {

class SetDPIUseCase {
public:
    SetDPIUseCase() = default;

    core::Result<void> execute(core::domain::IMouseAdapter* adapter, int target_dpi);
};

} // namespace mc::app::use_cases
