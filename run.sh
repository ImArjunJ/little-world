#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$project_dir" -B "$project_dir/build" \
    -DCMAKE_TOOLCHAIN_FILE="$project_dir/vendor/sengine/cmake/clang_libcxx.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_dir/build" --parallel
exec "$project_dir/build/little_world"
