#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

fail=0

echo "Checking local Markdown links..."

mapfile -t markdown_files < <(
    {
        printf '%s\n' README.md
        rg --files docs scripts third_party -g '*.md'
    } | sort -u
)

for markdown_file in "${markdown_files[@]}"; do
    [[ -f "$markdown_file" ]] || continue
    while IFS= read -r link; do
        [[ -n "$link" ]] || continue
        case "$link" in
            http://*|https://*|mailto:*|tel:*|'#'*) continue ;;
        esac

        # Markdown permits an optional title after the destination.
        link="${link%% \"*}"
        link="${link%%$'\t'*}"
        link="${link%%#*}"
        [[ -n "$link" ]] || continue

        if [[ "$link" == /* ]]; then
            target="$link"
        else
            target="$(dirname "$markdown_file")/$link"
        fi

        if [[ ! -e "$target" ]]; then
            echo "Broken local link: $markdown_file -> $link" >&2
            fail=1
        fi
    done < <(rg -o '\]\([^)]*\)' "$markdown_file" | sed -E 's/^\]\((.*)\)$/\1/')
done

check_forbidden_docs() {
    local label="$1"
    local pattern="$2"
    shift 2
    local matches

    matches="$(rg -n -i "$pattern" "$@" --glob '*.md' || true)"
    if [[ -n "$matches" ]]; then
        echo "Stale documentation pattern in $label:" >&2
        echo "$matches" >&2
        fail=1
    fi
}

check_forbidden_docs \
    "shell filesystem syntax" \
    'fs[[:space:]]+(pwd|ls)' \
    README.md docs scripts third_party

check_forbidden_docs \
    "HTTP method syntax" \
    'POST[[:space:]]+/api' \
    README.md docs scripts third_party

check_forbidden_docs \
    "removed FreeRTOS I2C source path" \
    'src/apps/freertos/drivers/pl/i2c' \
    README.md docs scripts third_party

# I2C has no autopoll fields. SMI keeps its own autopoll configuration, so the
# check is scoped to the old I2C HTTP example rather than the whole documentation.
check_forbidden_docs \
    "I2C HTTP autopoll fields" \
    'autopoll_(enabled|regs|reg_delay_ms|cycle_delay_ms)' \
    docs/user/http.md

if (( fail != 0 )); then
    exit 1
fi

echo "Documentation checks passed"
