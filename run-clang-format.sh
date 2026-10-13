#!/usr/bin/env bash
# Formats Metroid64's own sources (submodules are left alone).
# Default to clang-format-14; override CLANG_FORMAT to use another 14.x binary.
CLANG_FORMAT="${CLANG_FORMAT:-clang-format-14}"

find metroid64 -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | eval "xargs -0 $CLANG_FORMAT -i --verbose"
