#!/bin/sh

if test -x /usr/sbin/dsmetool; then
	for plugin in /usr/lib/mafw-plugin/*.so; do
		# For uninstall to work, we must start them using just the
		# basename without '.so'.
		plugin="${plugin##*/}";
		plugin="${plugin%.so}";
		/usr/bin/mafw.sh start "$plugin" &
	done
fi
