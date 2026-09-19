"""Offline projections of the two diagnostics; no native reuse is inferred."""
from collections import Counter
import hashlib
import json
from pathlib import Path
from time import perf_counter

root = Path.cwd()
record = root / 'docs/active/2026-09-15-verified-delivery'
out = root / 'out/verified-delivery'
load = lambda p: json.loads(p.read_text(encoding='utf-8'))
dump = lambda v: json.dumps(v, sort_keys=True, separators=(',', ':'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
response = load(record / 'B-response-receipt.json')
observations = load(out / 'B-observations.json')
started = perf_counter()
graphs = [load(root / p) for p in response['graphs']]
nodes, outgoing, spans = [], [], []
for graph in graphs:
    begin = len(nodes)
    ids = {node['id']: begin + i for i, node in enumerate(graph['nodes'])}
    for node in graph['nodes']:
        # The only omitted node field beyond the name is display expected cost.
        nodes.append({k: v for k, v in node.items() if k not in ('id', 'expected_cost')})
        outgoing.append([])
    for edge in graph['edges']:
        fields = {k: v for k, v in edge.items() if k not in ('id', 'from', 'to')}
        outgoing[ids[edge['from']]].append((fields, ids[edge['to']]))
    spans.append((begin, len(nodes)))

def intern(signatures):
    mapping = {}
    return [mapping.setdefault(s, len(mapping)) for s in signatures]

local = intern([dump(v) for v in nodes])
colors = local
rounds = 0
while True:
    updated = intern([dump([colors[i], [[e, colors[target]] for e, target in outgoing[i]]])
        for i in range(len(nodes))])
    rounds += 1
    if len(set(updated)) == len(set(colors)):
        colors = updated
        break
    colors = updated
    assert rounds <= len(nodes)

def overlap(labels):
    counters = [Counter(labels[a:b]) for a, b in spans]
    shared = set(counters[0]) & set(counters[1])
    return dict(shared_classes=len(shared),
        nodes_in_shared_classes=[sum(c[k] for k in shared) for c in counters],
        multiplicity_limited_matches=sum(min(counters[0][k], counters[1][k]) for k in shared))

shared_colors = set(colors[spans[0][0]:spans[0][1]]) & set(colors[spans[1][0]:spans[1][1]])
shared_nodes = [[node['id'] for i, node in enumerate(graph['nodes'])
    if colors[spans[g][0] + i] in shared_colors] for g, graph in enumerate(graphs)]

representation = []
for row in observations:
    # This projection serializes tag counts only; comparisons require zero tags.
    for req in [row['union'], *[n['required'] for n in row['per_node']]]:
        assert req['tag_count'] == 0 and all(a['tag_count'] == 0 for a in req['affixes'])
    representation.append(dict(path=row['path'], nodes=row['nodes'],
        nodes_equal_to_global_union=sum(n['required'] == row['union'] for n in row['per_node']),
        nodes_different_from_global_union=sum(n['required'] != row['union'] for n in row['per_node']),
        established_removed_global_classes=0,
        established_removed_membership_predicates=0,
        private_candidate_layout_replay=False))

result = dict(
    probe_sources={p.name: sha(p) for p in (record / 'probes').glob('*') if p.is_file()},
    raw_observations_sha256=sha(out / 'B-observations.json'),
    representation=representation,
    graph_control_syntax=dict(nodes=[len(g['nodes']) for g in graphs],
        edges=[len(g['edges']) for g in graphs], local_bodies=overlap(local),
        fixed_point=overlap(colors), refinement_rounds=rounds,
        shared_fixed_point_node_labels=shared_nodes,
        analysis_ms=(perf_counter()-started)*1000,
        semantics='Control syntax only: no member-domain/native-row/physical-entry correspondence.'),
    response=dict(comparisons=2, cold_evaluations=response['result'],
        native_semantic_pair_overlap=None, new_physical_entries=None,
        response_boundary_pairs=None, response_nonzeros=None,
        verified_reusable_physical_pairs_established=0,
        response_retained_bytes=None, response_peak_scratch_bytes=None,
        matching_native_dependencies_ms=None, response_build_ms=None, invalidation_ms=None,
        cold_pair_wall_ms=sum(r['wall_ms'] for r in response['result']),
        second_check_component_and_solve_ms=sum(response['result'][1][k]
            for k in ['components_ms', 'solve_ms', 'continuation_ms']),
        savings_established_ms=0),
    decision='neither',
    limitation='No smaller native layout or reusable physical interior was established. Unknown fields are not zero costs.')
(record / 'B-comparison.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: result[k] for k in ['representation', 'graph_control_syntax', 'decision']}))
