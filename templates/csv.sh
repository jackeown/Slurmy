#!/usr/bin/env bash
# Quote every field, preserving commas, quotes, newlines and empty values.
csv_row() {
    local field separator=
    for field in "$@"; do
        field=${field//\"/\"\"}
        printf '%s"%s"' "$separator" "$field"
        separator=,
    done
    printf '\n'
}
