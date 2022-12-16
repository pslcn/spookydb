# SpookyDB

To build SpookyDB, simply:

```bash
make
```

### Server Usage

To start the volume server:

```bash
./ukv-server start
```

To shutdown the server:

```bash
./ukv-server shutdown
```

### Usage

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

### Arch Notes

SpookyDB has one primary thread pool, which is used for request handling, and consists of 4 threads - meaning a maximum of 4 clients can concurrently operate on the database.
