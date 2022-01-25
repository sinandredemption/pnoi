sdptool search --bdaddr $1 OPUSH | grep Channel: | awk '{print $2}'
