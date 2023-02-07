# Syncronous Example
## Compilation Guide
By default, you would probably take these steps for linux distribution :
```
$ autoreconf -fvi
$ ./configure
$ make
$ make install
```

## Running Locally
```
$ ./src/syncronous [port]
```
## Test Using Curl
For example, if the server running in port 9001
```
$ curl http://127.0.0.1:9001/
{"status":"OK"}
```
Syncronous Terminal log :
```
[ <<< ] [127.0.0.1][/] 
[ >>> ] {"status":"OK"}
```

## WIKI Page

[libcsvr WIKI](https://github.com/yuharsenergi/libcsvr/wiki)
***
© Ergi, 2022
