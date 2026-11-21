#!/usr/bin/env bash
#
# Check (and optionally fix) include guards in header files.
#
# Expected format, e.g. for src/memory/config.h:
#
#   #ifndef INCLUDE_MEMORY_CONFIG
#   #define INCLUDE_MEMORY_CONFIG
#   ...
#   #endif // INCLUDE_MEMORY_CONFIG
#
# Usage:
#   scripts/check-include-guards.sh              # check all headers under src/
#   scripts/check-include-guards.sh --fix        # fix all headers under src/
#   scripts/check-include-guards.sh FILE...      # check specific headers
#   scripts/check-include-guards.sh --fix FILE... # fix specific headers
#
# The source root defaults to "src" and can be overridden with SRC_DIR.

set -uo pipefail

src_dir="${SRC_DIR:-src}"
fix=0
checked=0
failed=0
fixed=0

usage() {
    sed -n '4,16p' "$0" | sed -e 's/^# \{0,1\}//'
}

files=()
while (($# > 0)); do
    case "$1" in
        -f | --fix)
            fix=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        --)
            shift
            files+=("$@")
            break
            ;;
        -*)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            files+=("$1")
            shift
            ;;
    esac
done

if ((${#files[@]} == 0)); then
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(find "$src_dir" -type f -name '*.h' -print0 | sort -z)
fi

expected_guard() {
    local rel="${1#"$src_dir"/}"
    rel="${rel%.h}"
    # INCLUDE_<FOLDER>_<...>_<FILENAME>, uppercase, non-alphanumerics -> "_"
    printf 'INCLUDE_%s' "$rel" |
        sed -e 's/[^A-Za-z0-9]/_/g' |
        tr '[:lower:]' '[:upper:]'
}

# Sets PROBLEMS to the list of missing/malformed guard lines.
check_header() {
    local file=$1 guard=$2
    PROBLEMS=()

    if ! grep -qE "^#ifndef[[:space:]]+${guard}[[:space:]]*$" "$file"; then
        PROBLEMS+=("missing '#ifndef ${guard}'")
    fi

    if ! grep -qE "^#define[[:space:]]+${guard}[[:space:]]*$" "$file"; then
        PROBLEMS+=("missing '#define ${guard}'")
    fi

    if ! grep -qE "^#endif[[:space:]]+//[[:space:]]+${guard}[[:space:]]*$" "$file"; then
        PROBLEMS+=("missing '#endif // ${guard}'")
    fi

    ((${#PROBLEMS[@]} == 0))
}

# Normalizes the guard in $1 to $2, editing the file in place.
fix_header() {
    local file=$1 guard=$2
    local tmp
    tmp=$(mktemp) || return 1

    awk -v guard="$guard" '
        function extract_guard(s) {
            sub(/^#ifndef[ \t\r]+/, "", s)
            sub(/[ \t\r].*$/, "", s)
            return s
        }
        { lines[NR] = $0 }
        END {
            old = ""
            ndef = 0
            for (i = 1; i <= NR; i++) {
                if (lines[i] ~ /^#ifndef[ \t\r]/) {
                    old = extract_guard(lines[i])
                    ndef = i
                    break
                }
            }

            has_define = 0
            if (old != "") {
                for (i = 1; i <= NR; i++) {
                    if (lines[i] ~ ("^#define[ \t\r]+" old "([ \t\r]|$)")) {
                        has_define = 1
                        break
                    }
                }
            }

            out_n = 0
            if (ndef == 0) {
                # No include guard at all: wrap the whole header.
                out[++out_n] = "#ifndef " guard
                out[++out_n] = "#define " guard
                out[++out_n] = ""
                for (i = 1; i <= NR; i++) out[++out_n] = lines[i]
            } else {
                seen_define = 0
                for (i = 1; i <= NR; i++) {
                    line = lines[i]
                    if (i == ndef) {
                        out[++out_n] = "#ifndef " guard
                        if (!has_define) out[++out_n] = "#define " guard
                    } else if (has_define && !seen_define && line ~ ("^#define[ \t\r]+" old "([ \t\r]|$)")) {
                        out[++out_n] = "#define " guard
                        seen_define = 1
                    } else if (old != "" && line ~ /^#endif/ && index(line, old) > 0) {
                        out[++out_n] = "#endif // " guard
                    } else {
                        out[++out_n] = line
                    }
                }
            }

            # Normalize (or add) the closing #endif.
            last = 0
            for (i = out_n; i >= 1; i--) {
                if (out[i] ~ /[^ \t\r]/) {
                    last = i
                    break
                }
            }
            if (last == 0) {
                out[++out_n] = "#ifndef " guard
                out[++out_n] = "#define " guard
                out[++out_n] = "#endif // " guard
            } else if (out[last] ~ /^#endif([ \t\r]|$)/) {
                out[last] = "#endif // " guard
            } else {
                out[++out_n] = ""
                out[++out_n] = "#endif // " guard
            }

            for (i = 1; i <= out_n; i++) print out[i]
        }
    ' "$file" >"$tmp"

    if cmp -s "$file" "$tmp"; then
        rm -f "$tmp"
        return 0
    fi

    cat "$tmp" >"$file"
    rm -f "$tmp"
    return 0
}

for file in "${files[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "error: $file: no such file" >&2
        failed=$((failed + 1))
        continue
    fi

    checked=$((checked + 1))
    guard="$(expected_guard "$file")"

    if check_header "$file" "$guard"; then
        continue
    fi

    if ((fix)); then
        fix_header "$file" "$guard"
        if check_header "$file" "$guard"; then
            fixed=$((fixed + 1))
            echo "FIXED: $file (guard: $guard)"
            continue
        fi
        echo "FAIL: $file (could not be fixed, expected guard: $guard)" >&2
        for problem in "${PROBLEMS[@]}"; do
            echo "  - $problem" >&2
        done
        failed=$((failed + 1))
        continue
    fi

    failed=$((failed + 1))
    echo "FAIL: $file (expected guard: $guard)" >&2
    for problem in "${PROBLEMS[@]}"; do
        echo "  - $problem" >&2
    done
done

if ((checked == 0)); then
    echo "error: no header files found under '$src_dir'" >&2
    exit 1
fi

if ((failed > 0)); then
    echo "$failed of $checked header(s) have invalid include guards" >&2
    exit 1
fi

if ((fixed > 0)); then
    echo "OK: fixed $fixed of $checked header(s); all now have valid include guards"
else
    echo "OK: all $checked header(s) have valid include guards"
fi
