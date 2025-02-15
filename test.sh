#!/bin/bash

while true; do
  # Generate random ammonia values (between 0 and 30 with one decimal place)
  ammonia1=$(awk -v r=$RANDOM 'BEGIN {printf "%.1f", (r/1000)}')
  ammonia2=$(awk -v r=$RANDOM 'BEGIN {printf "%.1f", (r/1000)}')

  # Increment reading counter
  reading1=$((reading1+1))
  reading2=$((reading2+1))

  # Publish for Trash Can A (sensor_id: 1)
  mosquitto_pub -h broker.emqx.io -p 1883 -t "amoniac/sensor/1" \
    -m "{\"sensor_id\": 1, \"ammonia\": $ammonia1, \"reading\": $reading1}"

  # Publish for Trash Can B (sensor_id: 2)
  mosquitto_pub -h broker.emqx.io -p 1883 -t "amoniac/sensor/2" \
    -m "{\"sensor_id\": 2, \"ammonia\": $ammonia2, \"reading\": $reading2}"

  # Wait for 5 seconds before next reading
  sleep 1
done