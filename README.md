# libcsvr

## Developer's Note

Hi ! Thank you for visiting.  
I havent't added any release yet cause I am currently working in this project by now.  
But feel free to learn, and test this library.  

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

## Linking Guide

You can check the example in the example `Makefile.am` file here:
* [Asyncronous example](./example/asyncronous/src/Makefile.am)
* [Syncronous example](./example/syncronous/src/Makefile.am)

***
© Ergi, 2022
