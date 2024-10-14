#!/bin/sh

if [ -f "/userdata/jetkvm/devmode.enable" ]; then
	mkdir -p /userdata/dropbear/etc
	mkdir -p /userdata/dropbear/.ssh
	ln -s /userdata/dropbear/etc /etc/dropbear
	ln -s /userdata/dropbear/.ssh /root/.ssh
	#TODO: setup syslog instead of -E(log to stderr)
	dropbear -R -E
else
	# Kill Dropbear if it's already running
	killall dropbear
fi

