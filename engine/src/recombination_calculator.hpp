#pragma once
#include "recombination.hpp"
namespace poecraft {
void validate_random_recomb_goal_projection(const char*, std::size_t);
std::string calculate_random_recomb_goals_json(const RandomRecombPair&, const char*, std::size_t);
}
