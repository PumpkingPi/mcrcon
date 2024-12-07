Building and installing
-----------------------

### Prerequisites

- GCC compatible compiler
- make
- POSIX.1 support
   * getopt()
   * strcasecmp()
   * tcflush()

---

### Compiling

    cc -std=gnu99 -Wpedantic -Wall -Wextra -Wno-gnu-zero-variadic-macro-arguments -O2 -o mcrcon mcrcon.c
    
>[!NOTE]
>If you are compiling on Windows remember to link with winsock by adding `-lws2_32` to your compiler command line.

---

Or you can run **make**

    make           - compiles mcrcon
    make install   - installs compiled binaries and manpage to the system
    make uninstall - removes binaries and manpage from the system
    
    file install locations:
        /usr/local/bin/mcrcon
        /usr/local/share/man/man1/mcrcon.1

Makefile **install** and **uninstall** rules are not available on Windows.
