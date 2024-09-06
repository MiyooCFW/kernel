# Kernel MiyooCFW
Current version in use is 5.4.261 as of commit: [103ed3d](https://github.com/MiyooCFW/kernel/commit/103ed3d7e7ca6ed91582f79998c65f969611efdc)
## Build instructions:
Usually you have to be the root user and for stability reasons it is recommended to use uClibc:

- grab source & cd:
```
git clone https://github.com/MiyooCFW/kernel
cd kernel
```
- set environment variables inline with SDK location:
```
export PATH=$PATH:/opt/miyoo/bin
export ARCH=arm
export CROSS_COMPILE=arm-miyoo-linux-uclibcgnueabi-
```
- write configuration
```
make miyoo_defconfig
```
- build
```
make
```
- edit configuration if needed & rebuild:
```
make menuconfig
make clean
make
```
- grab output kernel modules/core & move to ``./dist``:
```
mkdir -p dist
mv arch/arm/boot/dts/*.dtb dist/
mv arch/arm/boot/zImage dist/
mv drivers/video/fbdev/core/*.ko dist/
mv drivers/video/fbdev/*.ko dist/
mv drivers/usb/gadget/legacy/*.ko dist/
mv sound/drivers/*.ko dist/
```

### Compile speed:
If you have a multicore CPU, you can increase build speed with:
```
make -j ${YOUR_CPU_COUNT}
```

---
# _Kernel 5.15.141_ (docs: https://www.kernel.org/doc/html/v5.15/)
---
