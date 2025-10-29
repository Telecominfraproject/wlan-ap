#!/bin/sh
PRIMARY_DIR="/root/mem_usage"
PRIMARY_FALLBACK_DIR="/tmp/mem_usage_live"
ARCHIVE_DIR="/tmp/mem_usage"
ARCHIVE_TMP_DIR="/tmp/mem_usage_tmp"

# thresholds
PRIMARY_MAX_BYTES=$((3 * 1024 * 1024))   # 3 MB
ARCHIVE_MAX_BYTES=$((15 * 1024 * 1024))  # 15 MB
RETENTION_DAYS=7                         # remove archives older than this
SLEEP_INTERVAL=10                       # 15 minutes between collections
KMEMLEAK_IFACE="/sys/kernel/debug/kmemleak"
# Ensure primary dir writable, otherwise fallback
if [ ! -d "$PRIMARY_DIR" ] || [ ! -w "$PRIMARY_DIR" ]; then
    mkdir -p "$PRIMARY_DIR" 2>/dev/null || true
fi
if [ ! -d "$PRIMARY_DIR" ] || [ ! -w "$PRIMARY_DIR" ]; then
    PRIMARY_DIR="$PRIMARY_FALLBACK_DIR"
    mkdir -p "$PRIMARY_DIR" 2>/dev/null || true
fi

# Ensure archive dir exists
mkdir -p "$ARCHIVE_DIR" 2>/dev/null || true
mkdir -p "$ARCHIVE_TMP_DIR" 2>/dev/null || true
mkdir -p "$PRIMARY_DIR" 2>/dev/null || true

# Host identity
MAC="$(uci get system.@system[0].hostname)"
MAC="${MAC:-}"

log() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >&2; }

dir_size_bytes() {
    t="$1"
    if [ ! -d "$t" ]; then echo 0; return 0; fi
    kb=$(du -s "$t" 2>/dev/null | awk '{print $1}')
    if [ -z "$kb" ]; then echo 0; else echo $((kb * 1024)); fi
}

# Ensure archive total size <= ARCHIVE_MAX_BYTES by deleting oldest tar.gz
enforce_archive_size_limit() {
    [ -d "$ARCHIVE_DIR" ] || return 0
    total=$(dir_size_bytes "$ARCHIVE_DIR")
    if [ "$total" -le "$ARCHIVE_MAX_BYTES" ]; then return 0; fi

    ls -1tr -- "$ARCHIVE_DIR"/*.tar.gz 2>/dev/null | while IFS= read -r af; do
        if [ -z "$af" ]; then break; fi
        rm -f -- "$af" && log "INFO: removed oldest archive $af to free space"
        total=$(dir_size_bytes "$ARCHIVE_DIR")
        if [ "$total" -le "$ARCHIVE_MAX_BYTES" ]; then break; fi
    done
}

# Create a single tarball containing ALL files from PRIMARY_DIR and move to ARCHIVE_DIR.
tar_all_primary_now() {
    ts=$(date +%Y%m%d_%H%M%S)
    tar_name="memtracker_all_${ts}.tar.gz"
    tmp_tar="${ARCHIVE_TMP_DIR}/${tar_name}.partial.$$"
    tar_err="${ARCHIVE_TMP_DIR}/tar_err_all.$$"

    if [ ! -d "$PRIMARY_DIR" ]; then
        echo "ERROR: PRIMARY_DIR '$PRIMARY_DIR' missing" >&2
        return 1
    fi

    set -- "$PRIMARY_DIR"/*
    if [ ! -e "$1" ]; then
        echo "DEBUG: no files in $PRIMARY_DIR to archive" >&2
        return 2
    fi

    (
        cd "$PRIMARY_DIR" || { echo "ERROR: cannot cd $PRIMARY_DIR" >&2; exit 3; }
        if ! tar -czf "$tmp_tar" . 2>"$tar_err"; then
            echo "ERROR: tar failed (see $tar_err)" >&2
            [ -f "$tmp_tar" ] && rm -f "$tmp_tar"
            exit 4
        fi
        exit 0
    )
    rc=$?
    if [ $rc -ne 0 ]; then
        return $rc
    fi

    if [ ! -d "$ARCHIVE_DIR" ]; then
        mkdir -p "$ARCHIVE_DIR" 2>/dev/null || {
            echo "WARN: cannot create ARCHIVE_DIR $ARCHIVE_DIR; leaving tar in $ARCHIVE_TMP_DIR" >&2
            mv -f "$tmp_tar" "${ARCHIVE_TMP_DIR}/${tar_name}" 2>/dev/null || true
            return 5
        }
    fi

    if mv -f "$tmp_tar" "$ARCHIVE_DIR/$tar_name" 2>/dev/null; then
        sync || true
        find "$PRIMARY_DIR" -maxdepth 1 -type f -print0 2>/dev/null |
        while IFS= read -r -d '' src; do rm -f -- "$src"; done
        echo "INFO: archived all -> $ARCHIVE_DIR/$tar_name" >&2
        return 0
    fi

    if cp -f "$tmp_tar" "$ARCHIVE_DIR/$tar_name" 2>/dev/null; then
        sync || true
        rm -f "$tmp_tar"
        find "$PRIMARY_DIR" -maxdepth 1 -type f -print0 2>/dev/null |
        while IFS= read -r -d '' src; do rm -f -- "$src"; done
        echo "INFO: copied archive -> $ARCHIVE_DIR/$tar_name (fallback)" >&2
        return 0
    fi

    mv -f "$tmp_tar" "${ARCHIVE_TMP_DIR}/${tar_name}" 2>/dev/null || true
    echo "WARN: failed to move/copy $tar_name to $ARCHIVE_DIR; kept ${ARCHIVE_TMP_DIR}/${tar_name}" >&2
    return 6
}

collect_meminfo() {
    ts=$(date +%Y%m%d_%H%M%S)
    out="$PRIMARY_DIR/meminfo_${ts}.csv"

    if [ ! -r /proc/meminfo ]; then
        echo "ERROR: Cannot read /proc/meminfo" >&2
        return 1
    fi

    {
        # header
        printf "timestamp,mac"
        awk '{gsub(":", "", $1); printf ",%s", $1}' /proc/meminfo
        printf "\n"

        # row of values
        printf "%s,%s" "$ts" "$MAC"
        awk '{printf ",%s", $2}' /proc/meminfo
        printf "\n"
    } > "$out"

    echo "Collected meminfo  -> $out"
}

collect_slabinfo() {
    ts=$(date +%Y%m%d_%H%M%S)
    out="$PRIMARY_DIR/slabinfo_${ts}.csv"

    [ -r /proc/slabinfo ] || { echo "ERROR: Cannot read /proc/slabinfo" >&2; return 1; }

    awk -v ts="$ts" -v mac="$MAC" '
    BEGIN {
        OFS = ","
        ncount = 0
    }
    /^slabinfo/ { next }
    /^#/ { next }
    {
        line = $0
        # split into up to 3 parts by ":" (some lines contain two ":" separators)
        parts_count = split(line, parts, ":")

        # trim leading/trailing whitespace from parts[1]
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", parts[1])

        # parse first segment tokens (name + first numeric columns)
        toks0_count = split(parts[1], t0, /[[:space:]]+/)
        name_raw = (toks0_count >= 1 ? t0[1] : "")
        # sanitize name to be CSV-safe
        name = name_raw
        gsub(/[^A-Za-z0-9_]/, "_", name)

        active      = (toks0_count >= 2 ? t0[2] : 0)
        num_objs    = (toks0_count >= 3 ? t0[3] : 0)
        objsize     = (toks0_count >= 4 ? t0[4] : 0)
        objperslab  = (toks0_count >= 5 ? t0[5] : 0)
        #pagesperslab= (toks0_count >= 6 ? t0[6] : 0)

        # tunables: parse parts[2] if present
        tun_limit = tun_batch = tun_shared = 0
        if (parts_count >= 2) {
            # trim whitespace
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", parts[2])
            ni = split(parts[2], t1, /[[:space:]]+/)
            # pick last 3 numeric tokens (limit, batchcount, sharedfactor)
            cnt = 0
            for (i = ni; i >= 1 && cnt < 3; i--) {
                if (t1[i] ~ /^[0-9]+$/) {
                    if (cnt == 0) tun_shared = t1[i]
                    else if (cnt == 1) tun_batch = t1[i]
                    else if (cnt == 2) tun_limit = t1[i]
                    cnt++
                }
            }
        }

        # slabdata: usually parts[3]; if missing, it may be in parts[2] - we try parts[3] first
        active_slabs = num_slabs = sharedavail = 0
        if (parts_count >= 3) {
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", parts[3])
            ni2 = split(parts[3], t2, /[[:space:]]+/)
            cnt2 = 0
            for (i = ni2; i >= 1 && cnt2 < 3; i--) {
                if (t2[i] ~ /^[0-9]+$/) {
                    if (cnt2 == 0) sharedavail = t2[i]
                    else if (cnt2 == 1) num_slabs = t2[i]
                    else if (cnt2 == 2) active_slabs = t2[i]
                    cnt2++
                }
            }
        } else if (parts_count == 2) {
            # fallback: try to extract slabdata numeric suffix from parts[2]
            ni2 = split(parts[2], t2, /[[:space:]]+/)
            cnt2 = 0
            for (i = ni2; i >= 1 && cnt2 < 3; i--) {
                if (t2[i] ~ /^[0-9]+$/) {
                    if (cnt2 == 0) sharedavail = t2[i]
                    else if (cnt2 == 1) num_slabs = t2[i]
                    else if (cnt2 == 2) active_slabs = t2[i]
                    cnt2++
                }
            }
        }

        # store values
        names[++ncount] = name
        vals[name, "active"] = active
        vals[name, "num_objs"] = num_objs
        vals[name, "objsize"] = objsize
        #vals[name, "objperslab"] = objperslab
        #vals[name, "pagesperslab"] = pagesperslab
        #vals[name, "tun_limit"] = tun_limit
        #vals[name, "tun_batch"] = tun_batch
        #vals[name, "tun_shared"] = tun_shared
        #vals[name, "active_slabs"] = active_slabs
        #vals[name, "num_slabs"] = num_slabs
        #vals[name, "sharedavail"] = sharedavail
    }
    END {
        # Header
        printf "timestamp%smac", OFS
        for (i = 1; i <= ncount; i++) {
            nm = names[i]
            printf "%s%s_active", OFS, nm
            printf "%s%s_num_objs", OFS, nm
            printf "%s%s_objsize", OFS, nm
            #printf "%s%s_objperslab", OFS, nm
            #printf "%s%s_pagesperslab", OFS, nm
            #printf "%s%s_tun_limit", OFS, nm
            #printf "%s%s_tun_batch", OFS, nm
            #printf "%s%s_tun_shared", OFS, nm
            #printf "%s%s_active_slabs", OFS, nm
            #printf "%s%s_num_slabs", OFS, nm
            #printf "%s%s_sharedavail", OFS, nm
        }
        printf "\n"

        # Values row
        printf "%s%s%s", ts, OFS, mac
        for (i = 1; i <= ncount; i++) {
            nm = names[i]
            printf "%s%s", OFS, (vals[nm, "active"] != "" ? vals[nm, "active"] : 0)
            printf "%s%s", OFS, (vals[nm, "num_objs"] != "" ? vals[nm, "num_objs"] : 0)
            printf "%s%s", OFS, (vals[nm, "objsize"] != "" ? vals[nm, "objsize"] : 0)
            #printf "%s%s", OFS, (vals[nm, "objperslab"] != "" ? vals[nm, "objperslab"] : 0)
            #printf "%s%s", OFS, (vals[nm, "pagesperslab"] != "" ? vals[nm, "pagesperslab"] : 0)
            #printf "%s%s", OFS, (vals[nm, "tun_limit"] != "" ? vals[nm, "tun_limit"] : 0)
            #printf "%s%s", OFS, (vals[nm, "tun_batch"] != "" ? vals[nm, "tun_batch"] : 0)
            #printf "%s%s", OFS, (vals[nm, "tun_shared"] != "" ? vals[nm, "tun_shared"] : 0)
            #printf "%s%s", OFS, (vals[nm, "active_slabs"] != "" ? vals[nm, "active_slabs"] : 0)
            #printf "%s%s", OFS, (vals[nm, "num_slabs"] != "" ? vals[nm, "num_slabs"] : 0)
            #printf "%s%s", OFS, (vals[nm, "sharedavail"] != "" ? vals[nm, "sharedavail"] : 0)
        }
        printf "\n"
    }' /proc/slabinfo > "$out" 2>/dev/null || { echo "ERROR: slabinfo parsing failed" >&2; return 2; }

    echo "Collected slabinfo  -> $out"
    return 0
}

collect_kmemleak() {
    ts=$(date +%Y%m%d_%H%M%S)
    out="$PRIMARY_DIR/kmemleak_${ts}.txt"

    if [ ! -d "$(dirname "$KMEMLEAK_IFACE")" ] || [ ! -e "$KMEMLEAK_IFACE" ]; then
        echo "WARN: kmemleak interface not present at $KMEMLEAK_IFACE" >&2
        return 1
    fi

    # Trigger kernel scan (best-effort; may require root)
    if [ -w "$KMEMLEAK_IFACE" ]; then
        # echo "scan" returns nothing; do it in a safe way
        echo scan > "$KMEMLEAK_IFACE" 2>/dev/null || true
        # small pause to let kernel produce a report (optional)
        sleep 1
    fi

    # Save the current kmemleak output (read-only)
    if [ -r "$KMEMLEAK_IFACE" ]; then
        # prefix with timestamp for clarity
        printf 'kmemleak snapshot: %s\n\n' "$ts" > "$out"
        cat "$KMEMLEAK_IFACE" >> "$out" 2>/dev/null || true
        sync || true
        echo "Collected kmemleak -> $out"
        return 0
    else
        echo "ERROR: cannot read $KMEMLEAK_IFACE" >&2
        return 2
    fi
}

log "Starting mem_monitor. PRIMARY_DIR=$PRIMARY_DIR ARCHIVE_DIR=$ARCHIVE_DIR"
while true; do

    collect_meminfo
    collect_slabinfo
    collect_kmemleak

    prim_size=$(du -s "$PRIMARY_DIR" 2>/dev/null | awk '{print $1}')
    prim_size=$(( prim_size * 1024 ))  # convert KB to bytes
    if [ "$prim_size" -ge "$PRIMARY_MAX_BYTES" ]; then
        echo "DEBUG: PRIMARY threshold reached: ${prim_size} bytes >= ${PRIMARY_MAX_BYTES}" >&2
        tar_all_primary_now || echo "WARN: tar_all_primary_now returned $?" >&2
    fi

    enforce_archive_size_limit

    sleep "$SLEEP_INTERVAL"
done
