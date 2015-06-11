#!/bin/sh

grep '^extern' include/waterline/waterline/*.h src/plugin_internal.h | sed -e 's/ *(.*//' -e 's/;.*//' | grep -o '[^ ]*$' | sort > symbols_from_headers

nm -P src/waterline | grep ' [TD] ' | grep -o '^[^ ]*' | sort > symbols_from_bin_waterline

diff symbols_from_headers symbols_from_bin_waterline
