#!/bin/sh

gtkdocize --docdir doc || exit 1

autoreconf -v -f -i || exit 1
test $NOCONFIGURE || ./configure --enable-debug --enable-maintainer-mode --enable-gtk-doc "$@"
