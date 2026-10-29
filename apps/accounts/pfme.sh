#!/bin/sh
set -eu
cd -- "$(dirname -- "$0")"
test -f .env || { echo 'Prepare the reviewed private .env first.' >&2; exit 1; }
python3 verify_images.py .env
dc() { docker compose --env-file .env "$@"; }
case "${1:-help}" in
  init)
    dc --profile admin run --rm --no-deps init
    dc --profile admin run --rm -T --no-deps admin manage.py migrate --noinput
    ;;
  secrets) dc --profile admin run --rm --no-deps secrets ;;
  friend)
    shift
    dc --profile admin run --rm --no-deps admin manage.py friend "$@"
    dc --profile admin run --rm -T --no-deps --user 0:980 admin manage.py sync_admission
    ;;
  start)
    test -f .public-activation-approved || { echo 'Live activation requires the final reviewed approval.' >&2; exit 1; }
    PFME_GATE_NONCE=$(python3 -c 'import secrets;print(secrets.token_hex(32))')
    export PFME_GATE_NONCE
    dc config --quiet
    dc stop connector
    dc --profile admin run --rm --no-deps init
    dc --profile admin run --rm -T --no-deps admin manage.py migrate --noinput
    dc --profile admin run --rm -T --no-deps --user 0:980 admin manage.py sync_admission
    activated=0
    trap 'if test "$activated" = 0; then dc --profile monitor stop connector accounts game status; fi' EXIT
    trap 'exit 1' HUP INT TERM
    dc up -d game accounts
    dc up -d --force-recreate connector
    if ! dc --profile admin run --rm --no-deps filter; then dc stop connector accounts game; exit 1; fi
    dc --profile monitor up -d status
    dc ps
    activated=1
    ;;
  stop) dc --profile monitor stop connector accounts game status ;;
  restart) sh "$0" stop; sh "$0" start ;;
  logs) dc logs --tail 80 game accounts connector ;;
  status) dc --profile monitor ps ;;
  homepage)
    test -f .public-activation-approved || exit 1
    dc --profile monitor exec -T status python -c "from pathlib import Path;print(Path('/run/secrets/status-token').read_text(),end='')" |
      python3 pfme_accounts/homepage_tile.py /home/jimbo/homepage/config/services.yaml
    ;;
  *) echo 'Commands: init, secrets, friend create/reset/disable/enable/list [username], start, stop, restart, logs, status, homepage' ;;
esac
