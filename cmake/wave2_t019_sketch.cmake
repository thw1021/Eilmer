# cmake/wave2_t019_sketch.cmake — T-019 (geom/misc/sketch.d + sketch_demo.d).
#
# Owns only its own target_sources(eil_misc ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
# Scope: geom/misc/sketch.d (964 D lines) -> src/geom/misc/sketch.{hpp,cpp},
# probes/geom/misc/probe_sketch.d, baselines/e587b235/probes/geom_misc_sketch.txt,
# tests/test_geom_misc_sketch.cpp, and sketch_demo.d -> tests/demo_geom_misc_sketch.cpp.
# rotating_a.d / drifting_eye.d / libplot.d are NOT here: no D binary can be linked for
# them on this machine (no GNU libplot), so there is no baseline to gate against — see
# reports/backlog.md T-921.
#
# Registration rule for this stage: the add_test() lines go in ONLY when the whole
# corpus replays at 0 differ. The gate is byte-equality over every row, so a partially
# generated corpus is not a passing test, and registering one would take the suite off
# green for every other stage sharing the tree.
