#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Tiny synthetic host tests; no APK, app, network or full runtime build.
set -euo pipefail
ulimit -c 0
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../control-files.sh"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/husk-control-tests.XXXXXX")
trap 'rm -rf -- "$WORK" "${HUSK_REGRESS_CONTROL_DIR:-}"' EXIT
mkdir "$WORK/tmp"
export TMPDIR="$WORK/tmp"
clang -std=c11 -fno-modules -Wall -Wextra -Werror "$HERE/host-control-test.c" -o "$WORK/host-control-test"
"$WORK/host-control-test" "$WORK"
out="$WORK/output with ' quote"
prepared=$(husk_regress_prepare_out "$out")
[ "$prepared" = "$out" ] && [ "$(/usr/bin/stat -f '%Lp' "$out")" = 700 ]
chmod 755 "$out"
husk_regress_prepare_out "$out" >/dev/null
[ "$(/usr/bin/stat -f '%Lp' "$out")" = 755 ]
chmod 777 "$out"
if husk_regress_prepare_out "$out" >/dev/null 2>&1; then echo 'unsafe OUT accepted' >&2; exit 1; fi
[ "$(/usr/bin/stat -f '%Lp' "$out")" = 777 ]
chmod 755 "$out"
ln -s "$out" "$WORK/linked-out"
if husk_regress_prepare_out "$WORK/linked-out" >/dev/null 2>&1; then exit 1; fi
umask 000
husk_regress_control_init
[ "$(/usr/bin/stat -f '%Lp' "$HUSK_REGRESS_CONTROL_DIR")" = 700 ]
ctl=$(husk_regress_control_write game 100 "$out/literal ' shot.png")
[ "$(/usr/bin/stat -f '%Lp' "$ctl")" = 600 ]
grep -F "shot $out/literal ' shot.png" "$ctl" >/dev/null
printf 'unchanged\n' > "$WORK/sentinel"
ln -s "$WORK/sentinel" "$HUSK_REGRESS_CONTROL_DIR/evil.ctl"
if husk_regress_control_write evil 100 ignored >/dev/null 2>&1; then exit 1; fi
[ "$(cat "$WORK/sentinel")" = unchanged ]
if husk_regress_control_write ../escape 100 ignored >/dev/null 2>&1; then exit 1; fi
if husk_regress_control_write newline 100 $'bad\nquit' >/dev/null 2>&1; then exit 1; fi
echo 'PASS: private control directory/files, noclobber symlink refusal, path validation, unchanged caller OUT modes/location'
