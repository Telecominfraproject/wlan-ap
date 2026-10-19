MM_RESET_PIN=$(cat /sys/kernel/debug/gpio | grep MM_RESET | sed -n 's/.*gpio-\([0-9]\+\).*/\1/p')
[ -n "$MM_RESET_PIN" ] && export MM_RESET_PIN=$MM_RESET_PIN
