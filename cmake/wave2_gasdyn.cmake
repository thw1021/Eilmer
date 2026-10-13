# cmake/wave2_gasdyn.cmake — Wave 2 slice of gasdyn (the CEA-free part).
#
# gasdyn/gasflow.d is NOT here: its shock relations take `ref GasState`/`GasModel`
# from the gas/ package (23,281 D lines, Wave 3) and gasflow_demo.d builds a model
# with init_gas_model() on a .lua file. See reports/backlog.md.
#
# This fragment is filled by the gasdyn stage: it declares `add_library(eil_gasdyn
# STATIC src/gasdyn/...)`, links eil_substrate + eil_nm, and registers one test per
# D module plus a demo whose stdout is diffed against the ldc2 capture.
