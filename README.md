# SpookyDB

## Client-Side Usage

Putting 'avalue' in 'akey':

```bash
curl --http0.9 -X PUT localhost:8080/akey -d avalue
```

Deleting key 'akey':

```bash
curl --http0.9 -X DELETE localhost:8080/akey
```

Getting value in 'akey' (now should be NULL):

```bash
curl --http0.9 localhost:8080/akey
```
