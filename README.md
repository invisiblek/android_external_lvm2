To build for android (tenderloin)

Instructions kanged from here:
https://github.com/steven676/android-lvm-mod/blob/master/HOWTO-BUILD

```bash
$ sudo apt-get install gcc-arm-linux-gnueabi

$ ./configure --prefix=/lvm --enable-static_link --disable-readline \
      --disable-selinux --with-pool=none --with-cluster=none \
      --with-confdir=/lvm/etc --with-default-run-dir=/data/lvm/run \
      --with-default-system-dir=/lvm/etc \
      --with-default-locking-dir=/data/lvm/lock \
      --with-optimisation="-Os -march=armv5te -mtune=cortex-a8 -mthumb"

$ export CC=arm-linux-gnueabi-gcc

$ ./configure --host=arm-linux-gnueabi \
      --prefix=/lvm --enable-static_link --disable-readline \
      --disable-selinux --with-pool=none --with-cluster=none \
      --with-confdir=/lvm/etc --with-default-run-dir=/data/lvm/run \
      --with-default-system-dir=/lvm/etc \
      --with-default-locking-dir=/data/lvm/lock \
      --with-optimisation="-Os -march=armv5te -mtune=cortex-a8 -mthumb"

```

Make these changes before compiling in lib/misc/configure.h:
* change DEFAULT_SYS_DIR to /lvm (we want to hard-code this)
* comment out lines relating to rpl_malloc and rpl_realloc

```bash
$ make
```
