#!/bin/sh

set -eu

contents_path=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$contents_path/MacOS/uxnemu" --wait "$@"
