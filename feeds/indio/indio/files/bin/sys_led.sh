#!/bin/sh

# Maximum number of retries
max_retries=5

# Delay between retries (in seconds)
retry_delay=1

# Function to turn on the LED (green)
turn_on_green_led() 
{
	echo "1" > /sys/class/leds/led_2g/brightness
	echo "0" > /sys/class/leds/led_5g/brightness      
	echo "0" > /sys/class/leds/led_sys/brightness

}

# Function to turn on the LED (red)
turn_on_blue_led() 
{
	echo "1" > /sys/class/leds/led_2g/brightness
	echo "1" > /sys/class/leds/led_5g/brightness      
	echo "0" > /sys/class/leds/led_sys/brightness
}

turn_on_pink_led()
{

	echo "0" > /sys/class/leds/led_2g/brightness
	echo "1" > /sys/class/leds/led_5g/brightness
	echo "1" > /sys/class/leds/led_sys/brightness
}

# Function to check internet connectivity
check_internet() 
{
	if ping -q -c 1 -W 1 8.8.8.8 > /dev/null; then
#        	echo "Internet is working"
        	return 0
    	else
#        	echo "Internet is not working"
        	return 1
    	fi
}

# Main loop to continuously check internet connectivity
while true; do
	# Attempt to check internet connectivity with retries
	attempt=1
	while [ $attempt -le $max_retries ]; do
		#echo "Attempt $attempt:"
		if check_internet; then
		    turn_on_green_led
		    break  # Exit the retry loop if internet is working
		fi
		attempt=$(( attempt + 1 ))
		sleep $retry_delay

	done

	# If all attempts fail, turn on red LED and continue loop
	if [ $attempt -gt $max_retries ]; then
	turn_on_blue_led
	fi

	# Sleep before the next iteration
	sleep 10
done

