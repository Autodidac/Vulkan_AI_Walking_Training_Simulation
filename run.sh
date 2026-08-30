#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runner="$script_dir/Runner"

if [ ! -x "$runner" ]; then
    printf '%s\n' "ERROR: The packaged Runner executable is missing beside run.sh." >&2
    exit 1
fi

cd "$script_dir"
exec "$runner" "$@"
