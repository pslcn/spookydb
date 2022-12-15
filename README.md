# ukeyvalue

### Usage

```bash
# Put 'avalue' in 'akey'
curl --http0.9 -X PUT localhost:8080/akey -d avalue

# Get value in 'akey'
curl --http0.9 localhost:8080/akey

# Delete key 'akey'
curl --http0.9 -X DELETE localhost:8080/akey
```
