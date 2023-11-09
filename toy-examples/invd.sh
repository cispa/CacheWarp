#!/bin/bash
INTERVAL=50
REPEAT=5
STEP=10

for i in {0..1023};
do
	echo "set: ${i}"
	for j in {1..$REPEAT};
	do
		sudo ./blind $INTERVAL $STEP ${i} 0
        sudo ./blind $INTERVAL $STEP ${i} 1
	done
done

