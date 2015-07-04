#!/bin/sh

grep '^extern' include/waterline/waterline/*.h src/plugin_internal.h | sed -e 's/ *(.*//' -e 's/;.*//' | grep -o '[^ ]*$' | sort > symbols_from_headers

nm -P src/waterline | grep ' [TD] ' | grep -o '^[^ ]*' | egrep -v '^(_start|main|_fini|_init|_edata|__libc_csu_fini|__libc_csu_init|__data_start)$' | sort > symbols_from_bin_waterline

diff symbols_from_headers symbols_from_bin_waterline


cat symbols_from_headers symbols_from_bin_waterline | sort | uniq > symbols_all

count_prefixed_symbols()
{
    printf "prefix %s : %s\n" "$1" "`grep '^'$1 symbols_all | wc -l`"
}

echo '================================='
count_prefixed_symbols wtl_
count_prefixed_symbols wtl_button_
count_prefixed_symbols wtl_command_
count_prefixed_symbols wtl_util_
count_prefixed_symbols wtl_x11_
count_prefixed_symbols panel_
count_prefixed_symbols plugin_
echo '================================='

egrep -v '^(wtl|plugin|panel)_' symbols_all
