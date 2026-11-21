#!/usr/bin/env bash
set -euo pipefail

repository=${1:?Usage: deploy-release.sh OWNER/REPO}
if [[ ! $repository =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]]; then
    echo "Expected a GitHub repository in OWNER/REPO form" >&2
    exit 1
fi

cd "$(dirname -- "$0")/.."
umask 077
exec 9> .noteworthy-release.lock
flock -n 9 || exit 0

tmp_dir=$(mktemp -d)
trap 'rm -rf -- "$tmp_dir"' EXIT

curl --fail --silent --show-error --retry 2 --max-time 30 \
    "https://api.github.com/repos/$repository/releases/latest" > "$tmp_dir/release.json"
tag=$(python3 -c 'import json, sys; print(json.load(sys.stdin)["tag_name"])' < "$tmp_dir/release.json")
if [[ ! $tag =~ ^noteworthy-production-([0-9a-f]{40})$ ]]; then
    exit 0
fi
revision=${BASH_REMATCH[1]}
for marker in .noteworthy-release .noteworthy-release-failed; do
    if [[ -f $marker && $(cat "$marker") == "$tag" ]]; then
        exit 0
    fi
done

release_url="https://github.com/$repository/releases/download/$tag"
for asset in images.tar.gz.sha256 images.tar.gz; do
    curl --fail --silent --show-error --location --retry 3 --max-time 240 \
        --output "$tmp_dir/$asset" "$release_url/$asset"
done
read -r expected_checksum _ < "$tmp_dir/images.tar.gz.sha256"
read -r actual_checksum _ < <(sha256sum "$tmp_dir/images.tar.gz")
if [[ $actual_checksum != "$expected_checksum" ]]; then
    echo "Image archive checksum did not match the release" >&2
    exit 1
fi

docker load --input "$tmp_dir/images.tar.gz"
docker image inspect "noteworthy-server:$revision" "noteworthy-qt:$revision" >/dev/null
previous_server=$(docker image inspect --format '{{.Id}}' noteworthy-server:local)
previous_qt=$(docker image inspect --format '{{.Id}}' noteworthy-qt:local)

activate() {
    docker tag "$1" noteworthy-server:local &&
        docker tag "$2" noteworthy-qt:local &&
        docker compose --profile staging up -d --no-deps --no-build noteworthy-server noteworthy-qt
}

healthy() {
    local status
    for attempt in {1..15}; do
        status=$(curl -sS --max-time 3 -o /dev/null -w '%{http_code}' \
            http://127.0.0.1:8080/v1/rooms/NOPE0 || true)
        if [[ $status == 404 ]] &&
            curl -fsSI --max-time 3 http://127.0.0.1:8084/noteworthy-qt.wasm |
                grep -qi '^content-type: application/wasm'; then
            return 0
        fi
        sleep 2
    done
    return 1
}

if activate "noteworthy-server:$revision" "noteworthy-qt:$revision" && healthy; then
    printf '%s\n' "$tag" > .noteworthy-release.new
    mv -- .noteworthy-release.new .noteworthy-release
    rm -f .noteworthy-release-failed
else
    echo "Release $tag failed its local checks. Restoring the previous images." >&2
    printf '%s\n' "$tag" > .noteworthy-release-failed
    activate "$previous_server" "$previous_qt"
    exit 1
fi
