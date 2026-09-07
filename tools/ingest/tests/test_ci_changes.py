from poecraft_ingest.ci_changes import classify


def test_prose_only_and_unknown_changes():
    assert not classify(['HANDOFF.md','docs/solver/research.md'])['native_required']
    for paths in (None, [], ['engine/src/owner.cpp'], ['docs/a.md','engine/a.cpp'],
                  ['.github/workflows/windows.yml'], ['scripts/build.ps1'], ['data/a.md'],
                  ['experiments/case.json'], ['apps/web/README.md'], ['docs/../engine/a.md']):
        assert classify(paths)['native_required']


def test_rename_and_comment_metadata_remain_conservative():
    assert classify(['engine/old.cpp','docs/new.md'])['native_required']
    assert not classify(['docs/old.md','docs/new.md'])['native_required']
    # A C++ diff cannot be classified as comment-only from its filename.
    assert classify(['engine/src/only_math_annotations.cpp'])['native_required']
