#!/bin/sh

rcK()
{
	for i in $(ls /oem/usr/etc/init.d/S??*) ;do

		# Ignore dangling symlinks (if any).
		[ ! -f "$i" ] && continue

		case "$i" in
			*.sh)
				# Source shell script for speed.
				(
					trap - INT QUIT TSTP
					set stop
					. $i
				)
				;;
			*)
				# No sh extension, so fork subprocess.
				$i stop
				;;
		esac
	done
}

echo "Stop Application ..."
killall jetkvm_app
killall udhcpc

while [ 1 ];
do
	sleep 1
	ps|grep jetkvm_app|grep -v grep
	if [ $? -ne 0 ]; then
		echo "jetkvm_app exit"
		break
	else
		echo "jetkvm_app active"
	fi
done

rcK
