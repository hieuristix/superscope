# Compile superscope with ray cast on
superscope:
    cmake -B build -DSS_RAYCAST=ON
    cmake --build build --target superscope test_capture_raycast -j

# Compile viewer
viewer:
    cmake -B build
    cmake --build build --target viewer -j

# Compile targets (e.g. `just com superscope` or `just com viewer`)
com target:
    just {{target}}

# Run targets (e.g. `just run superscope` or `just run viewer`)
run_superscope: superscope
    ./build/superscope/superscope

run_viewer: viewer
    ./build/viewer/bin/viewer

run target:
    just run_{{target}}
