#!/bin/sh

GTKDOCIZE="gtkdocize --flavour no-tmpl"
LIBTOOLIZE="libtoolize -f"
ACLOCAL="aclocal"
AUTOHEADER="autoheader -f"
AUTOCONF="autoconf -f"
AUTOMAKE="automake -f -a --foreign"

run()
{
    command="$1"

    output=$($command 2>&1)

    if test $? -ne 0
    then
        command=$(echo "$command" | cut -d ' ' -f 1)

        echo "$command failed:" >&2
        echo "$output" >&2

        exit 1
    fi
}

curdir=$(pwd)
srcdir=$(dirname "$0")

cd "$srcdir" || exit 1

run "$GTKDOCIZE"
run "$LIBTOOLIZE"
run "$ACLOCAL"
run "$AUTOHEADER"
run "$AUTOCONF"
run "$AUTOMAKE"

cd "$curdir" || exit 1

"$srcdir/configure" "$@" || exit $?

exit 0
