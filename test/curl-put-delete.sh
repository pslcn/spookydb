#!/bin/sh

for i in {1..4}
do
	curl -X PUT localhost:8080/akey -d avalue
	curl -X DELETE localhost:8080/akey
done
