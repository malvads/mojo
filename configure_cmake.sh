#!/usr/bin/env bash
set -eo pipefail
IFS=$'\n\t'

me="$(basename "$0")"
sourcedir="$(cd "$(dirname "$0")" && pwd)"

builddir="build"
prefix="/usr/local"
CMakeCacheEntries=()
CMakeGenerator=""
build_type=""
install_after_build=false
cores=""
cmake_args=()

command -v cmake >/dev/null 2>&1 || {
    echo "cmake is required but not installed"
    exit 1
}

append_cache_entry() {
    CMakeCacheEntries+=("-D$1:$2=$3")
}

detect_cores() {
    if [ -n "$cores" ]; then
        echo "$cores"
        return
    fi
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif sysctl -n hw.ncpu >/dev/null 2>&1; then
        sysctl -n hw.ncpu
    else
        echo 4
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --builddir=*)
            builddir="${1#*=}"
            ;;
        --generator=*)
            CMakeGenerator="${1#*=}"
            ;;
        --prefix=*)
            prefix="${1#*=}"
            append_cache_entry "CMAKE_INSTALL_PREFIX" "PATH" "$prefix"
            ;;
        --build-type=*)
            build_type="${1#*=}"
            ;;
        --install)
            install_after_build=true
            ;;
        --cores=*)
            cores="${1#*=}"
            ;;
        --define=*)
            CMakeCacheEntries+=("-D${1#*=}")
            ;;
        --)
            shift
            while [ $# -gt 0 ]; do
                cmake_args+=("-D$1")
                shift
            done
            break
            ;;
        *)
            if [[ "$1" == *=* ]]; then
                cmake_args+=("-D$1")
            else
                echo "Invalid option: $1"
                exit 1
            fi
            ;;
    esac
    shift
done

if [ -z "$build_type" ]; then
    if [ -z "${CFLAGS-}" ] && [ -z "${CXXFLAGS-}" ]; then
        build_type="RelWithDebInfo"
    else
        build_type="Debug"
    fi
fi

case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        echo "Invalid build type: $build_type"
        exit 1
        ;;
esac

mkdir -p "$builddir"
cd "$builddir"

rm -f CMakeCache.txt

gen_args=()

if [ -n "$CMakeGenerator" ]; then
    gen_args=(-G "$CMakeGenerator")
elif command -v ninja >/dev/null 2>&1; then
    gen_args=(-G Ninja)
fi

cmake_cmd=(cmake)

if [ ${#gen_args[@]} -gt 0 ]; then
    cmake_cmd+=("${gen_args[@]}")
fi

cmake_cmd+=(
  "-DCMAKE_BUILD_TYPE=$build_type"
  "-DCMAKE_INSTALL_PREFIX=$prefix"
  "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  "-DCMAKE_C_FLAGS=${CFLAGS-} ${CPPFLAGS-}"
  "-DCMAKE_CXX_FLAGS=${CXXFLAGS-} ${CPPFLAGS-}"
)

if [ ${#CMakeCacheEntries[@]} -gt 0 ]; then
    cmake_cmd+=("${CMakeCacheEntries[@]}")
fi

if [ ${#cmake_args[@]} -gt 0 ]; then
    cmake_cmd+=("${cmake_args[@]}")
fi

cmake_cmd+=("$sourcedir")

echo "Configuring..."
"${cmake_cmd[@]}"

cat > config.status <<EOF
$me $*
EOF

chmod u+x config.status || true

echo "Building..."

num_cores="$(detect_cores)"

cmake --build . -- -j"$num_cores"

if [ "$install_after_build" = true ]; then
    echo "Installing..."
    cmake --install .
fi

echo "Done. Build directory: $builddir"
