# libcsvr

## Developer's Note

Hi ! Thank you for visiting.  
I am currently working in this project by now.  
Feel free to learn, and test this library.  

## Compilation Guide
I am using autoconf compilation tools to create the shared library and static library. By default, you would probably take these steps for linux distribution which outputs will be installed inside the `/usr/lib` directory :
```
$ autoreconf -fvi
$ ./configure
$ make
$ make install
```

But I suggest to create your own `/path/to/your/libcsvr/output/directory` to use this library, and linking your application using the created shared / static shared file. For example :
```
$ autoreconf -fvi
$ ./configure --prefix=/path/to/your/libcsvr/output/directory
$ make
$ make install
```

If you want to use the TLS mode, you must define the environment variable `WITH_TLS` and the ssl link `-lssl` during configuring :
```
$ ./configure --prefix=/path/to/your/libcsvr/output/directory CPPFLAGS=-DWITH_TLS LDFLAGS=-lssl
```

## Linking Guide
You can check the linking example in the example `Makefile.am` file here:
* [Asyncronous Makefile.am](./example/asyncronous/src/Makefile.am)
* [Syncronous Makefile.am](./example/syncronous/src/Makefile.am)
* [Syncronous with TLS Makefile.am](./example/syncronous/src/Makefile.am)

## How to Use
You can check the example in the example below, all sources are provided in the `src` directory:
* [Asyncronous](./example/asyncronous/)
* [Syncronous](./example/syncronous/)
* [Syncronous with TLS](./example/syncronous/)

## WIKI Page

[libcsvr WIKI](https://github.com/yuharsenergi/libcsvr/wiki)
***
© Ergi, 2022
