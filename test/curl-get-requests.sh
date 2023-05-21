#!/bin/sh

while true 
do
	for i in {1..6}
	do
		curl --http0.9 localhost:8080/Hello &
	done
done
