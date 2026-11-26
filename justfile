# Compile superscope with ray cast on
superscope:
    cmake -B build -DSS_RAYCAST=ON
    cmake --build build --target superscope test_capture_raycast -j

# Compile viewer
viewer:
    cmake -B build
    cmake --build build --target viewer -j

# Compile server / client / launcher
server:
    cmake -B build
    cmake --build build --target server -j

client:
    cmake -B build
    cmake --build build --target client -j

launcher:
    cmake -B build
    cmake --build build --target server launcher -j

# Build container image and Linux server/client binaries
container:
    container build -t superscope-dev .
    container run --rm -v "{{justfile_directory()}}:/src" -w /src superscope-dev sh -c "cmake -B build-linux -DSS_NET_ONLY=ON && cmake --build build-linux --target server client -j"

# Compile/build targets (e.g. `just compile superscope`, `just build container`)
compile target:
    just {{target}}

build target:
    just {{target}}

# Run targets (e.g. `just run superscope` or `just run viewer`)
run_superscope: superscope
    ./build/superscope/superscope

run_viewer: viewer
    ./build/viewer/bin/viewer

run_client: client
    ./build/superscope/client

run_launcher: launcher
    ./build/superscope/launcher

run_container name id:
    container run -d --name {{name}} -v "{{justfile_directory()}}:/src" superscope-dev /src/build-linux/superscope/server --id {{id}}

run_container_client:
    container run -it --rm -v "{{justfile_directory()}}:/src" -w /src superscope-dev /src/build-linux/superscope/client

run target *args:
    just run_{{target}} {{args}}

# Kill targets (e.g. `just kill all`, `just kill all server`, `just kill all client`)
kill_all_server:
    -pkill -f "build/superscope/server"
    -container delete --force $(container ls -a -q)

kill_all_client:
    -pkill -f "build/superscope/client"

kill_all_all: kill_all_server kill_all_client

kill action target="all":
    just kill_{{action}}_{{target}}
