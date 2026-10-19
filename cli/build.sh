#!/bin/bash
set -e
find src/ -type f | xargs -x clang-format -i
find tests/ -type f "*.cc" | xargs -x clang-format -i

BUILD_DIR=".build"
COVERAGE=false
CLEAN=false
EXTRA_CMAKE=""

# Parse flags
for arg in "$@"; do
    case "$arg" in
        --coverage|-c) COVERAGE=true ;;
        --clean|-C)    CLEAN=true ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "  --coverage, -c    Build with LLVM coverage and generate report"
            echo "  --clean, -C       Remove build directory first"
            echo "  --help, -h        Show this help"
            exit 0
            ;;
        *) EXTRA_CMAKE="$EXTRA_CMAKE $arg" ;;
    esac
done

# Coverage build needs Clang — must rebuild from scratch
if [ "$COVERAGE" = true ]; then
    CLEAN=true
    BUILD_DIR=".build-coverage"
fi

if [ "$CLEAN" = true ]; then
    rm -rf "$BUILD_DIR"
    echo "Cleaned $BUILD_DIR"
fi

install -d "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$COVERAGE" = true ]; then
    echo "==> Coverage build (Clang + -fprofile-instr-generate)"
    cmake .. \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCOVERAGE=ON \
        $EXTRA_CMAKE
    cmake --build . -- -j "$(nproc)"
    echo ""
    echo "==> Running tests to collect coverage data..."
    LLVM_PROFILE_FILE="default.profraw" tests/test_cases
    echo ""
    echo "==> Merging raw profiles..."
    llvm-profdata merge -sparse default.profraw -o default.profdata
    echo ""
    echo "==> Coverage summary (source files only):"
    llvm-cov report tests/test_cases \
        -instr-profile=default.profdata \
        -ignore-filename-regex="tests/|\.h$" \
        2>/dev/null
    echo ""
    echo "==> HTML report: $BUILD_DIR/coverage_html/index.html"
    llvm-cov show tests/test_cases \
        -instr-profile=default.profdata \
        -ignore-filename-regex="tests/" \
        -format=html \
        -output-dir=coverage_html \
        2>/dev/null
else
    cmake .. -DCMAKE_CXX_COMPILER=g++ $EXTRA_CMAKE
    cmake --build . -- -j "$(nproc)"
    echo
    tests/test_cases 2>&1 | grep -wE "Failure|FAILED" || true
fi
