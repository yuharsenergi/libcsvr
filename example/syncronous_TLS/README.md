# Syncronous_TLS Example
## Compilation Guide
To use the TLS mode, you must define the environment variable `WITH_TLS` and during configuring. The ssl link `-lssl` is optional as long as during configurin the libcsvr the `-lssl` already included.  
By default, you would probably take these steps for linux distribution :
```
$ autoreconf -fvi
$ ./configure CPPFLAGS=-DWITH_TLS LDFLAGS=-lssl
$ make
$ make install
```

## Generate private key
I have created simple tools to generate unsigned certificate key. For Example :
```
$ ./certificate/generate.sh server.key server.pem
```

## Running Locally
```
$ ./src/syncronous_tls [port] [certificate key] [private key]
```
## Test Using Curl
For example, if the server running in port 9001, add `-k` flag to skip SSL verifications in curl :
```
$ curl https://127.0.0.1:9001/ -k
{"status":"OK","type":1,"message":"Hello!"}
```
Asyncronous Terminal log :
```
[ <<< 127.0.0.1:54356] 
[ >>> ] {"status":"OK","type":1,"message":"Hello!"}
```
## WIKI Page

[libcsvr WIKI](https://github.com/yuharsenergi/libcsvr/wiki)
***
© Ergi, 2022
