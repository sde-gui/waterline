#!/bin/sh

set -xe

AUTOPOINT="${AUTOPOINT:-autopoint}"
INTLTOOLIZE="${INTLTOOLIZE:-intltoolize}"
AUTORECONF="${AUTORECONF:-autoreconf}"

$AUTOPOINT --force || exit $?
export AUTOPOINT="$INTLTOOLIZE --automake --copy"
$AUTORECONF --force --install --verbose

rm -rf autom4te.cache
