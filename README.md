# SpookyDB

## Server-Side Usage

To start the key-value database server, simply:

```bash
make
./server
```

At its current stage of development, to kill the server you have to manually stop the process (using something like htop, for example).

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
