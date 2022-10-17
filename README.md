# FIABLE DATAGRAM PROTOCOL
## Compile
`make`
## Run
### Generate file
`head -c 1G </dev/urandom >/tmp/random.data` \
`cksum testfile.txt`
### client1 - Scénario 1
`bin/client1 10.43.5.20 3000 random.data`
### client2 - Scénario 2
`bin/client2 10.43.5.20 3000 random.data`
### client3 - Scénario 3 multiclient
`bin/client2 10.43.5.20 3000 random.data`