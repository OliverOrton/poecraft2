#pragma once

class ProductionPolicyOracle final : public PolicyRefinementOracle {
#include "solver_policy_oracle_setup.inc"
#include "solver_policy_oracle_kernels.inc"
#include "solver_policy_oracle_mapping.inc"
#include "solver_policy_oracle_choices.inc"
#include "solver_policy_oracle_observations.inc"
#include "solver_policy_oracle_lift.inc"
};
