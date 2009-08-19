#!/bin/sh

if test -x /usr/sbin/dsmetool; then
	for plugin in /usr/lib/mafw-plugin/*.so; do
		# For uninstall to work, we must start them using just the
		# basename without '.so'.
		plugin="${plugin##*/}";
		plugin="${plugin%.so}";
                nice="";
                if test "x$plugin" = "xmafw-gst-renderer"; then
                    # Run the renderer with a nice value
                    nice="-7";
                fi
		sudo /usr/bin/mafw.sh start "$plugin" $nice &
        done
fi
