#pragma once

#include "solver_compile_contracts.hpp"
#include "solver_eval_types.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace poecraft::solver {

enum class SelectiveCompletionVariant : std::uint8_t {
    RetentionControl,
    RerollVersusRepair,
    ProtectedScour,
    ProtectedScourFill,
    EldritchGrowthRepair,
    EldritchGrowthWithBlocker,
    PartialHeldRecoveryResearch, // No product/Finder proposal enumerates this.
};

struct SelectiveCompletionCandidate {
    FinderControlGraph control;
    std::uint32_t acquisition_action = kNoId;
    double acquisition_price = 0.0;
    SelectiveCompletionVariant variant =
        SelectiveCompletionVariant::RetentionControl;
};

SelectiveCompletionVariant product_completion_variant(const CalcContext& problem,
    std::uint32_t held_side = kNoId);
bool product_completion_has_two_orientations(const CalcContext& problem);
std::uint32_t product_completion_proposal_count(const CalcContext& problem);
SelectiveCompletionVariant product_completion_proposal_variant(
    const CalcContext& problem, std::uint32_t proposal);
std::uint32_t product_completion_held_side(const CalcContext& problem,
    std::uint32_t orientation);

/* Candidate construction only. The caller owns exact evaluation, reached
 * programme-entry validation, incumbent selection and proof authority. One
 * producer has one original-root problem and at most one live admission cursor.
 * Each advance performs at most one unit of native automatic admission. */
class SelectiveCompletionProducer {
  public:
    SelectiveCompletionProducer(
        CalcContext& problem, const pc_item_state& original_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& limits, SelectiveCompletionVariant variant,
        std::uint32_t acquisition_action = kNoId,
        std::uint32_t held_side = kNoId);

    bool advance(std::uint32_t max_work_items = 1);
    bool done() const { return phase_ == Phase::Done; }
    const std::optional<SelectiveCompletionCandidate>& candidate() const {
        return candidate_;
    }
    const std::string& status() const { return status_; }
    std::uint64_t estimated_owned_bytes() const;

  private:
    enum class Phase : std::uint8_t {
        Begin, Primary, PrimaryDirect, Secondary, SecondaryDirect,
        Tertiary, TertiaryDirect, Blocker, Build, Done,
    };
    struct Programme {
        ActionType intended = ActionType::Chaos;
        std::uint32_t source = kNoId;
        std::uint32_t initial = kNoId;
        std::uint32_t ready = kNoId;
        std::uint32_t direct = kNoId;
    };

    CalcContext& problem_;
    pc_item_state original_start_{};
    const std::unordered_map<std::string, double>& prices_;
    SolveOptions limits_;
    SelectiveCompletionVariant variant_;
    // Proposal selectors may bind a native acquisition descriptor and side.
    // Full graph evaluation and reached-entry validation own acceptance.
    std::uint32_t requested_acquisition_ = kNoId;
    std::uint32_t requested_held_side_ = kNoId;
    // Non-default construction is accessible only to the private research owner.
    friend class PartialHeldRecoveryProducer;
    std::uint32_t requested_held_mask_ = 0;
    bool reroll_without_target_progress_ = false;
    bool guard_missing_goal_rollability_ = false;
    bool escape_persistent_blockers_ = false;
    bool full_side_exalt_fill_ = false;
    Phase phase_ = Phase::Begin;
    std::string status_ = "pending";
    std::optional<SelectiveCompletionCandidate> candidate_;
    std::array<std::vector<std::uint32_t>, 2> side_slots_;
    std::uint32_t held_side_ = PC_SIDE_PREFIX;
    std::uint32_t target_side_ = PC_SIDE_SUFFIX;
    std::uint32_t held_mask_ = 0;
    std::uint32_t acquisition_action_ = kNoId;
    double acquisition_price_ = 0.0;
    std::uint32_t fill_action_ = kNoId;
    Programme primary_;
    Programme secondary_;
    Programme tertiary_;
    Programme blocker_;
    AutomaticAdmissionLimits admission_;

    void begin();
    bool advance_programme(Programme& programme, bool direct,
                           std::uint32_t max_work_items);
    void build();
    void refuse(std::string reason);
};

/* Disabled construction experiment. Neither Current nor Finder creates this
 * producer or enumerates its anchors. The explicit private gate is test-owned;
 * a candidate has no checked upper, closure or publication authority.
 * Two singleton anchors and four sequential existing stage templates bound the
 * grammar; there is still at most one live native admission cursor. */
struct PartialHeldRecoveryScope {
    std::uint32_t held_side = kNoId; // The two-goal side.
    std::array<std::uint32_t, 2> anchor_masks{};
};
std::optional<PartialHeldRecoveryScope> partial_held_recovery_scope(
    const CalcContext& problem, const pc_item_state& original_start,
    const SolveOptions& limits);

class PartialHeldRecoveryProducer {
  public:
    PartialHeldRecoveryProducer(CalcContext& problem,
        const pc_item_state& original_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& limits, std::uint32_t anchor_mask,
        bool private_gate = false, bool guard_missing_rollability = true,
        bool escape_persistent_blockers = true, bool full_side_exalt_fill = false);
    bool advance(std::uint32_t max_work_items = 1);
    bool done() const { return done_; }
    const std::optional<SelectiveCompletionCandidate>& candidate() const {
        return candidate_;
    }
    const std::string& status() const { return status_; }
    std::uint64_t estimated_owned_bytes() const;

  private:
    CalcContext& problem_;
    pc_item_state original_start_{};
    const std::unordered_map<std::string, double>& prices_;
    SolveOptions limits_;
    std::uint32_t anchor_mask_ = 0;
    std::optional<PartialHeldRecoveryScope> scope_;
    std::uint32_t acquisition_ = kNoId;
    double acquisition_price_ = 0;
    std::uint32_t growth_ready_tiers_ = kNoId;
    std::uint32_t final_original_ready_tiers_ = kNoId;
    std::uint32_t final_growth_ready_tiers_ = kNoId;
    std::uint32_t stage_ = 0;
    bool begun_ = false;
    bool done_ = false;
    bool private_gate_ = false;
    bool guard_missing_rollability_ = true;
    bool escape_persistent_blockers_ = true;
    bool full_side_exalt_fill_ = false;
    std::string status_ = "pending";
    std::unique_ptr<SelectiveCompletionProducer> active_;
    std::array<std::optional<SelectiveCompletionCandidate>, 4> stages_;
    std::optional<SelectiveCompletionCandidate> candidate_;
    void refuse(std::string reason);
    void begin();
    void begin_stage();
    void finish_stage();
    void compose();
};

/* Exact reached-entry admission checker for a compiled control. Finder and
 * Current use one native rule and one resumable admission cursor. The root
 * evaluator supplies the immutable census; this checks each positive entry. */
class SelectiveProgrammeEntryValidator {
  public:
    SelectiveProgrammeEntryValidator(
        const CalcContext& problem,
        std::shared_ptr<const SessionImpl> session,
        const FinderControlGraph& control,
        const StrategyPolicyEntryCertificate& census,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& limits);

    bool advance(std::uint32_t max_work_items = 1);
    bool done() const { return cursor_ >= census_.entries.size(); }
    std::uint32_t validated_entries() const { return cursor_; }
    std::uint32_t positive_entries() const { return positive_entries_; }
    std::uint64_t logical_work() const;
    std::uint64_t active_work() const;
    std::uint64_t estimated_owned_bytes() const;
    // Finite diagnostic comparison only: the retained census stays borrowed.
    // Existing consumers retain the full ownership calculation until qualified.
    std::uint64_t fast_estimated_owned_bytes() const;
    std::uint64_t audited_estimated_owned_bytes() const;

  private:
    const CalcContext& problem_;
    std::shared_ptr<const SessionImpl> session_;
    const FinderControlGraph& control_;
    const StrategyPolicyEntryCertificate& census_;
    const std::unordered_map<std::string, double>& prices_;
    SolveOptions limits_;
    // Declared before the child so child/admission destruction comes first.
    // Only unowned or more tightly capped callers need a private forwarding owner.
    std::unique_ptr<CalcContext> work_budget_;
    std::unique_ptr<CalcContext> calc_;
    AutomaticAdmissionLimits admission_;
    std::size_t cursor_ = 0;
    std::uint32_t state_ = kNoId;
    std::uint32_t positive_entries_ = 0;
};

} // namespace poecraft::solver
