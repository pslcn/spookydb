#!/bin/sh

# curl --http0.9 -X PUT localhost:8080/akey -d avalue
# curl --http0.9 -X DELETE localhost:8080/akey

for i in {1..4}
do
	curl --http0.9 -X PUT localhost:8080/akey -d avalue
	curl --http0.9 -X DELETE localhost:8080/akey
done
