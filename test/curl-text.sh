#!/bin/sh

curl --http0.9 -X PUT localhost:8080/akey -d avalue
curl --http0.9 -X DELETE localhost:8080/akey
curl --http0.9 localhost:8080/akey
