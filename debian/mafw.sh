#!/bin/sh
# /usr/bin/mafw.sh <start|stop> PLUGIN
# Not supposed to work anywhere but on the device.

if test -z "$1" -o -z "$2"; then
	echo "usage: $0 <start|stop> PLUGIN [NICE]";
	exit 1;
fi
wrapper='/usr/bin/mafw-dbus-wrapper';
test -x $wrapper || exit 0;
dsmetool='/usr/sbin/dsmetool';
test -x $dsmetool || exit 0;
if test -f /etc/osso-af-init/af-defines.sh; then
        source /etc/osso-af-init/af-defines.sh 2>/dev/null;
        export HOME='/home/user';
fi
nicearg=""
if test "x$3" != "x"; then
        nicearg="-n $3"
fi
case $1 in
	start)  $dsmetool -U user -f "$wrapper $2" $nicearg || true;;
	stop)   $dsmetool -U user -k "$wrapper $2" || true;;
esac
exit 0;
