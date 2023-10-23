#!/bin/sh

curl -X PUT localhost:8080/akey -d avalue
curl localhost:8080/Hello
curl -X DELETE localhost:8080/akey
curl localhost:8080/akey
curl -X DELETE localhost:8080/Hello
