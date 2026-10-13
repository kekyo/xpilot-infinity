#!/bin/sh

set -eu

fail()
{
	echo "resolve-build-metadata.sh: $*" >&2
	exit 1
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
resolved_version=${XPILOT_VERSION:-}
resolved_commit=${XPILOT_COMMIT_ID:-}

if test -z "$resolved_version" || test -z "$resolved_commit"; then
	command -v npx >/dev/null 2>&1 \
		|| fail "npx was not found; install Node.js and npm"
	resolved_metadata=$(
		cd "$project_root"
		# The native repository root intentionally has no package.json.
		printf '%s\n' '{version}' '{git.commit.hash}' \
			| npx --yes screw-up format -f "$@" \
			| tr -d '\r'
	)
	screw_up_version=$(printf '%s\n' "$resolved_metadata" | sed -n '1p')
	screw_up_commit=$(printf '%s\n' "$resolved_metadata" | sed -n '2p')
	extra_metadata=$(printf '%s\n' "$resolved_metadata" | sed -n '3p')
	test -z "$extra_metadata" \
		|| fail "screw-up returned unexpected metadata"
	test -n "$resolved_version" || resolved_version=$screw_up_version
	test -n "$resolved_commit" || resolved_commit=$screw_up_commit
fi

case "$resolved_version" in
	''|*[!0-9A-Za-z.+:~-]*) fail "invalid version: $resolved_version" ;;
esac
case "$resolved_commit" in
	*[!0-9a-f]*) fail "invalid commit ID: $resolved_commit" ;;
esac
test "${#resolved_commit}" -eq 40 \
	|| fail "commit ID is not a full SHA-1 hash: $resolved_commit"

printf '%s\n' "$resolved_version" "$resolved_commit"
