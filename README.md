# Kernel MiyooCFW

## Build instructions:
Usually you have to be the root user and for stability reasons it is recommended to use uClibc:

- grab source & cd:
```
git clone https://github.com/MiyooCFW/kernel
cd kernel
```
- set environment variables:
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
mv arch/arm/boot/dts/suniv-f1c500s-miyoo.dtb dist/
mv arch/arm/boot/zImage dist/
mv drivers/video/fbdev/core/*.ko dist/
mv drivers/video/fbdev/*.ko dist/
```

### Compile speed:
If you have a multicore CPU, you can increase build speed with:
```
make -j ${YOUR_CPU_COUNT}
```

---
# _Kernel 4.14.0_ (docs: https://www.kernel.org/doc/html/v4.14/)
---
