#!/bin/sh
# Run the host unit tests (make test) in a container with a host Qt 5, on a
# machine that has Docker (default: the ssh host "docker").
# Usage: tools/host-test.sh [ssh-host]
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
host=${1:-docker}
remote=build/kosync-test
image=kosync-hosttest
cd "$root"
tar czf - --exclude=./.git --exclude=./build --exclude='*.o' --exclude='*.moc' --exclude='*.so' \
    --exclude=KoboRoot.tgz --exclude=tests/units_test --exclude=tests/fixture.epub . |
    ssh "$host" "rm -rf $remote && mkdir -p $remote && tar xzf - -C $remote"
ssh "$host" "cd $remote && (docker image inspect $image >/dev/null 2>&1 || docker build -q -t $image -f tools/Dockerfile.hosttest tools) \
    && docker run --rm -v \"\$PWD:/src\" -w /src $image make test"
