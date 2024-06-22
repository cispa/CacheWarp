#!/bin/bash
INTERVAL=50
REPEAT=2
STEP=10

for i in {0..1023};
do
	echo "set: ${i}"
	for j in {1..$REPEAT};
	do
		sudo ./cachewarp_blind_drop $INTERVAL $STEP ${i} 0
		#sudo ./cachewarp_blind_drop $INTERVAL $STEP ${i} 1
	done
done


