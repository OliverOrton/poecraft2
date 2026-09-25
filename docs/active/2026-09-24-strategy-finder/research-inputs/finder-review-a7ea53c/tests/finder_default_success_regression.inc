/* NOT EXECUTED IN THIS REVIEW.
 * Integrate into engine/tests/test_solver_compile.cpp's existing anonymous
 * namespace and call next to run_finder_request_binding_tests().
 * Uses its existing make_compile_session() fixture; not a standalone target.
 * Current source is expected to fail the final negative assertion.
 */
void run_finder_default_success_regression() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot wanted;
    wanted.family_id = session->family_id.at(5);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    CalcContext calc(session, goal, registry, {chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    PC_CHECK(!calc.is_goal_state(calc.state(calc.intern_item(start))));

    const std::string graph =
        "{\"version\":\"v1\",\"name\":\"default goal decoration\","
        "\"base_state\":{\"base_key\":\"synthetic/base\","
        "\"item_level\":1,\"rarity\":\"rare\",\"with_implicits\":false,"
        "\"prefixes\":[],\"suffixes\":[]},"
        "\"start_node_id\":\"start\",\"nodes\":["
        "{\"id\":\"start\",\"kind\":\"start\"},"
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"}],"
        "\"edges\":[{\"id\":\"decorated_default\",\"from\":\"start\","
        "\"to\":\"goal\",\"priority\":0,\"is_default\":true,"
        "\"condition\":" + compile_finder_goal_condition(calc) + "}]}";

    // Ordinary default semantics should remain unchanged. The finder boundary
    // must reject this proposal, rather than reinterpret its original bytes.
    const auto compiled = compile_strategy_json(
        session, graph.data(), graph.size());
    PC_CHECK(compiled->nodes.at(compiled->start_node).edges.size() == 1);
    const auto& edge = compiled->nodes.at(compiled->start_node).edges.front();
    PC_CHECK(edge.is_default);
    PC_CHECK(edge.condition.kind == ConditionKind::Always);
    const auto prepared = prepare_finder_candidate(calc, session, start, graph);
    PC_CHECK(!prepared.ready());
}
