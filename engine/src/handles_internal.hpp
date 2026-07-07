#ifndef POECRAFT_SRC_HANDLES_INTERNAL_HPP
#define POECRAFT_SRC_HANDLES_INTERNAL_HPP

#include <memory>

#include "engine_internal.hpp"

/*
 * Opaque C-handle bodies, shared by every translation unit that implements
 * public API surface (api.cpp, solver_api.cpp). One definition here keeps
 * the reinterpret casts ODR-clean.
 */
struct pc_data {
    std::shared_ptr<poecraft::DataImpl> impl;
};
struct pc_session {
    std::shared_ptr<poecraft::SessionImpl> impl;
};
struct pc_action_context {
    std::unique_ptr<poecraft::ActionContextImpl> impl;
};
struct pc_strategy {
    std::shared_ptr<poecraft::StrategyImpl> impl;
};
struct pc_economy {
    std::shared_ptr<poecraft::EconomyImpl> impl;
};
struct pc_simulator {
    std::unique_ptr<poecraft::SimulatorImpl> impl;
};

#endif
