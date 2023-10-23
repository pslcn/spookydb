#!/bin/sh

while true 
do
	for i in {1..6}
	do
		curl localhost:8080/Hello &
	done
done
