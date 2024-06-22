#!/bin/bash
INTERVAL=50
REPEAT=2
STEP=2

for i in {0..1023};
do
	echo "set: ${i}"
	for j in {1..$REPEAT};
	do
		sudo ./blind $INTERVAL $STEP ${i} 0
	done
done

