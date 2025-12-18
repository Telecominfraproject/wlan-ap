#!/bin/sh

CACHE=/tmp/.country_last

supported_boards="
sonicfi,rap630w-311g
sonicfi,rap630c-311g
"
board=$( [ -e /tmp/sysinfo/board_name ] && cat /tmp/sysinfo/board_name || echo "generic" )
echo "$supported_boards" | grep -F -q -x "$board" || return 0

read_vals() {
    printf "%s|%s\n" \
        "$(uci -q get wireless.radio0.country)" \
        "$(uci -q get wireless.radio1.country)"
}

while :; do
    NOW=$(read_vals)
    case "$NOW" in
        *'|'*)
            NEW0=${NOW%|*}
            NEW1=${NOW#*|}
            [ -n "$NEW0" ] && [ -n "$NEW1" ] && break
            ;;
    esac
    sleep 1
done

while :; do
    NOW=$(read_vals)
    BEFORE=$(cat "$CACHE")
    if [ "$NOW" != "$BEFORE" ]; then
        OLD0=$(echo "$BEFORE" | cut -d'|' -f1)
        OLD1=$(echo "$BEFORE" | cut -d'|' -f2)
        NEW0=$(echo "$NOW"  | cut -d'|' -f1)
        NEW1=$(echo "$NOW"  | cut -d'|' -f2)
        TO_SET=
        [ "$NEW0" != "$OLD0" ] && TO_SET="$TO_SET $NEW0"
        [ "$NEW1" != "$OLD1" ] && TO_SET="$TO_SET $NEW1"
        for c in $(echo $TO_SET | tr ' ' '\n' | sort -u); do
            /usr/bin/set_country.sh "$c"
        done
        echo "$NOW" > "$CACHE"
    fi
    sleep 1
done
