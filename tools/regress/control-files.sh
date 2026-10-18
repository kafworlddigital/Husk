#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Sourced by run.sh; does not start a game, build or download.
husk_regress_prepare_out() {
    local out=$1 mode
    if [ ! -e "$out" ] && [ ! -L "$out" ]; then
        (umask 077; mkdir -p "$out") || return 1
    fi
    if [ ! -d "$out" ] || [ -L "$out" ] || [ ! -O "$out" ]; then
        echo "regress: output must be an owned directory, not a symlink: $out" >&2; return 1
    fi
    mode=$(/usr/bin/stat -f '%Lp' "$out") || return 1
    if (( (8#$mode & 8#22) != 0 )); then
        echo "regress: output directory must not be group/world writable: $out" >&2; return 1
    fi
    (cd "$out" && pwd -P)
}
husk_regress_control_init() {
    HUSK_REGRESS_CONTROL_DIR=$(umask 077; mktemp -d "${TMPDIR:-/tmp}/husk-regress-ctl.XXXXXX") || return 1
}
husk_regress_control_write() {
    local name=$1 delay=$2 shot=$3 ctl mode
    case "$name" in ''|.|..|*[!a-zA-Z0-9_.-]*) echo 'regress: invalid control name' >&2; return 1;; esac
    case "$shot" in *$'\n'*|*$'\r'*) echo 'regress: screenshot path cannot contain a newline' >&2; return 1;; esac
    if [ ! -d "${HUSK_REGRESS_CONTROL_DIR:-}" ] || [ -L "$HUSK_REGRESS_CONTROL_DIR" ] ||
       [ ! -O "$HUSK_REGRESS_CONTROL_DIR" ]; then
        echo 'regress: private control directory unavailable' >&2; return 1
    fi
    mode=$(/usr/bin/stat -f '%Lp' "$HUSK_REGRESS_CONTROL_DIR") || return 1
    if (( (8#$mode & 8#77) != 0 )); then
        echo 'regress: control directory must be private' >&2; return 1
    fi
    ctl="$HUSK_REGRESS_CONTROL_DIR/$name.ctl"
    (umask 077; set -o noclobber; printf 'wait %d\nshot %s\nquit\n' "$delay" "$shot" > "$ctl") || return 1
    printf '%s\n' "$ctl"
}
