# SpookyDB

Shell scripts with curl commands are in `test/`. 

## Client-Side Usage

Putting 'avalue' in 'akey':

```bash
curl -X PUT localhost:8080/akey -d avalue
```

Deleting key 'akey':

```bash
curl -X DELETE localhost:8080/akey
```

Getting value in 'akey' (now should be NULL):

```bash
curl localhost:8080/akey
```
