#!/bin/sh

JK_SYSTEM_MAC_FILE="/userdata/.mac_address"
JK_USER_MAC_FILE="/userdata/jetkvm/mac_address"

JK_USER_MAC_FILE2="/data/ethaddr.txt"

get_mac_from_i2c() {
	local chip_address="${1:-50}"
	mac=""
	for reg in fa fb fc fd fe ff; do
		value=$(i2cget -y 1 $chip_address 0x$reg)
		# check if return code is 0
		if [ $? -ne 0 ]; then
			echo "00:00:00:00:00:00"
			return 1
		fi

		value=$(echo "$value" | sed 's/0x//')
		mac="${mac}${value}"
	done

	mac=$(echo "$mac" | tr '[:lower:]' '[:upper:]')
	mac=$(echo "$mac" | sed 's/.\{2\}/&:/g; s/:$//')

	echo "$mac"
}

create_new_random_mac() {
	# Generates a locally administered MAC: 02:XX:XX:XX:XX:XX
	octets=$(hexdump -n5 -e '5/1 "%02X "' /dev/urandom)
	set -- $octets
	echo "02:$1:$2:$3:$4:$5"
}

write_mac_address_to_i2c() {
	ITER=0
	for reg in fa fb fc fd fe ff; do
		byte=$(echo "$1" | cut -d':' -f$ITER)
		echo "jetkvm-i2c: writing to register 0x$reg: 0x$byte"
		i2cset -y 1 "0x50" "0x$reg" "0x$byte"
		echo "jetkvm-i2c: register 0x$reg written"
		ITER=$((ITER + 1))
	done
	echo "jetkvm-i2c: MAC address written to i2c"
}

fix_i2c_mac_address() {
	echo "jetkvm-macgen: I2C MAC address is [FF:FF:FF:FF:FF:FF], calculating new MAC address based on cpuid hash"
	local cpuid=$(cat /proc/cpuinfo | grep -m 1 "Serial" | awk '{print $3}')
	local cpuid_hash=$(echo -n "$cpuid" | sha1sum | awk '{print $1}')
	# get the last 5 characters of the cpuid hash
	local cpuid_hash_last_5=${cpuid_hash: -5}

	# the range for devices without proper MAC population is 30-52-53-00-FF-FF to 30-52-53-0F-FF-FF 
	# so we need to convert the cpuid hash to a number and then to a mac address

	local mac_address="3052530${cpuid_hash_last_5}"
	local mac_address_formatted=$(echo "$mac_address" | sed 's/.\{2\}/&:/g; s/:$//')

	echo "jetkvm-macgen: new MAC address is: [$mac_address_formatted]"


	# i2c might be deprecated in favor of eeprom in the future
	# so we won't write the mac address to i2c here now

	# instead, we write to /userdata/.mac_address
	echo "$mac_address_formatted" > "$JK_SYSTEM_MAC_FILE"
}

is_valid_i2c_mac_address() {
	# Microchip MAC address ranges
	local acceptable_ranges="
00:04:A3
D8:80:39
00:1E:C0
54:10:EC
80:1F:12
04:91:62
68:27:19
E8:EB:1B
80:34:28
60:8A:10
FC:0F:E7
9C:95:6E
44:B7:D0
D8:47:8F
40:84:32
"
	# check if mac address is valid
	if ! echo "$1" | grep -qE '^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$' ; then
		return 1
	fi

	# get first 3 bytes of the mac address
	local mac_prefix=$(echo "$1" | cut -d':' -f1-3)
	# check if the mac prefix is in the acceptable ranges
	if echo "$acceptable_ranges" | grep -qEi "^$mac_prefix" ; then
		return 0
	fi
	return 1
}

set_up_mac_address() {
	local mac_address=""

	i2c_mac_address=$(get_mac_from_i2c)
	if ! is_valid_i2c_mac_address "$i2c_mac_address" ; then
		fix_i2c_mac_address
		# i2c_mac_address=$(get_mac_from_i2c)
		# if [ "$i2c_mac_address" = "FF:FF:FF:FF:FF:FF" ]; then
		# 	echo "jetkvm: unable to program mac address to i2c, will fallback to user-defined mac address"
		# fi
	fi

	# https://github.com/jetkvm/kvm/issues/375#issuecomment-2836029895
	if [ -f "$JK_USER_MAC_FILE2" ]; then
		mac_address=$(cat "$JK_USER_MAC_FILE2")
		echo "jetkvm: found user-defined MAC address in [$JK_USER_MAC_FILE2]: [$mac_address], moving it to [$JK_USER_MAC_FILE]"
		mv "$JK_USER_MAC_FILE2" "$JK_USER_MAC_FILE"
	fi

	# get mac address from file
	if [ -f "$JK_USER_MAC_FILE" ]; then
		mac_address=$(cat "$JK_USER_MAC_FILE")
		echo "jetkvm: user-defined MAC address: [$mac_address]"
	elif [ -f "$JK_SYSTEM_MAC_FILE" ]; then
		mac_address=$(cat "$JK_SYSTEM_MAC_FILE")
		echo "jetkvm: MAC address from file: [$mac_address]"
	else
		mac_address=$i2c_mac_address
		echo "jetkvm: I2C MAC address: [$mac_address]"
	fi

	# verify if it's valid and make sure it's not all ff or 00
	if ! echo "$mac_address" | grep -qE '^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$' || \
		[ "$mac_address" = "FF:FF:FF:FF:FF:FF" ] || \
		[ "$mac_address" = "00:00:00:00:00:00" ]; then 
		# generate a random mac address
		mac_address=$(create_new_random_mac)
		echo "jetkvm: no valid mac address found, using random mac: [$mac_address]"
		echo "$mac_address" > "$JK_USER_MAC_FILE"
	fi

	# set mac address
	echo "jetkvm: setting mac address of [eth0] to: [$mac_address]"
	ifconfig eth0 hw ether $mac_address
}
