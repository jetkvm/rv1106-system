#!/bin/sh

. ./jetkvm-lib.sh

test_is_valid_i2c_mac_address() {
	local success_cases="44:b7:d0:d5:72:00 00:04:A3:00:00:00"
	local failure_cases="00:00:00:00:00:00 ff:ff:ff:ff:ff:ff 44:b7:d0:dX:72:99"
	for mac in $success_cases; do
		echo "+ test_is_valid_i2c_mac_address: [$mac] should be valid"
		is_valid_i2c_mac_address "$mac"
		if [ $? -ne 0 ]; then
			echo "!! expected 0 but got $?"
			exit 1
		fi
	done

	for mac in $failure_cases; do
		echo "+ test_is_valid_i2c_mac_address: [$mac] should be invalid"
		is_valid_i2c_mac_address "$mac"
		if [ $? -ne 1 ]; then
			echo "!! expected 1 but got $?"
			exit 1
		fi
	done
}

test_get_mac_from_i2c() {
	local success_chip_addresses="50"
	local failure_chip_addresses="51 aa"
	for chip_address in $success_chip_addresses; do
		echo "+ test_get_mac_from_i2c: [$chip_address]"
		local mac_address=$(get_mac_from_i2c $chip_address)
		echo "mac_address: [$mac_address]"
		if [ $mac_address == "00:00:00:00:00:00" ]; then
			echo "!! expected a valid mac address but got [$mac_address]"
			exit 1
		fi
	done
	for chip_address in $failure_chip_addresses; do
		echo "+ test_get_mac_from_i2c: [$chip_address]"
		local mac_address=$(get_mac_from_i2c $chip_address)
		echo "mac_address: [$mac_address]"
		if [ $mac_address != "00:00:00:00:00:00" ]; then
			echo "!! expected [00:00:00:00:00:00] but got [$mac_address]"
			exit 1
		fi
	done
}

test_is_valid_i2c_mac_address
test_get_mac_from_i2c