# 一键编译
在/AnyCloudV500_PDK_V1.02/PDK/SDK/sdk_release_dir下执行`./auto_build.sh`  

>Type of chip : AK3760D (64M RAM)  
Type of screen : RGB  
Type of flash : spi nor flash  
Wifi card : rtl8188ftv  

可能的依赖
```
sudo apt-get install liblzo2-dev:i386  
sudo apt-get install liblzma5:i386  
sudo apt-get install liblzma-dev  
sudo apt-get install libncurses5-dev  
sudo apt-get install libx11-dev:i386 libreadline6-dev:i386  
sudo apt-get install?build-essential  
sudo apt-get install lib32stdc++6  
sudo apt-get install lib32z1  
```

burnfile中为编译生成的文件`uImage`、`anyka_ev500.dtb`、`u-boot.bin`、`root.sqsh4`、`usr.jffs2`、`usr.sqsh4`，用burntool烧录需要将6个文件全部复制到burntool目录下。不要用uart_burntool。  

# NFS挂载
在`/etc/exports`  最后添加

    /home/nfs_share *(rw,sync,no_root_squash,no_subtree_check) 

重启NFS  

    sudo /etc/init.d/nfs-kernel-server restart  

登陆板子终端  
查看串口号`ls -l /dev/ttyUSB*  `  
username : root  
password : anycloudv500  
先运行`nfs_start.sh`，然后进行挂载，根据情况修改IP和主机目录。

    mount -t nfs -o nolock 192.168.1.104:/home/nfs_share /mnt  

交叉编译工具链`arm-anykav500-linux-uclibcgnueabi-gcc`

## 修改根文件系统
在`/AnyCloudV500_patch2-1/PDK/SDK/sdk_release_dir/platform/rootfs/rootfs.tar.gz`中进行修改，修改完成后重新编译和烧录。  
在`/usr/sbin/nfs_start.sh`最后添加挂载指令，就不用每次手动输入了。
>mount -t nfs -o nolock 192.168.1.104:/home/nfs_share /mnt

# Qt移植
主要步骤参考`【正点原子】I.MX6U Qt移植V1.0.pdf`  ，下面是一些要修改的步骤（待更新）
## 编译Tslib
安装automake工具  

    sudo apt-get install autoconf automake libtool  pkg-config

指定编译工具及输出路径

    ./configure --host=arm-anykav500-linux-uclibcgnueabi --cache-file=tmp.cache --prefix=/home/bk/Qt_transplant/arm-tslib CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc

在根文件系统中改好`/etc/profile`重新编译烧录并source以后，需要调用一下显示屏例程`ak_vo_sample`，然后才能运行`./mnt/arm-tslib/bin/ts_test`不知道为什么，而且触摸点左右颠倒。`ts_calibrate`由于文件系统只读暂时没效果。

## 编译Qt
autoconfigure.sh配置
```
#!/bin/sh
./configure \
-prefix /home/bk/Qt_transplant/arm-qt \
-confirm-license \
-opensource \
-shared \
-release \
-make libs \
-xplatform linux-arm-gnueabi-g++ \
-optimized-qmake \
-pch \
-qt-sql-sqlite \
-qt-libjpeg \
-qt-libpng \
-qt-zlib \
-no-opengl \
-no-sse2 \
-no-openssl \
-no-cups \
-no-glib \
-no-dbus \
-no-xcb \
-no-xcursor -no-xfixes -no-xrandr -no-xrender \
-no-separate-debug-info \
-no-fontconfig \
-nomake examples -nomake tools -nomake tests -no-iconv \
-tslib \
-I/home/bk/Qt_transplant/arm-tslib/include \
-L/home/bk/Qt_transplant/arm-tslib/lib
exit
```
qmake.comf配置
```
#
# qmake configuration for building with arm-linux-gnueabi-g++
#

MAKEFILE_GENERATOR      = UNIX
CONFIG                 += incremental
QMAKE_INCREMENTAL_STYLE = sublib

QT_QPA_DEFAULT_PLATFORM = linuxfb
#QMAKE_CFLAGS += -O2 -march=armv5tej -mtune=ARM926EJ-S -mfpu=neon -mfloat-abi=hard
#QMAKE_CXXFLAGS += -O2 -march=armv5tej -mtune=ARM926EJ-S -mfpu=neon -mfloat-abi=hard

QMAKE_CFLAGS += -msoft-float -D__GCC_FLOAT_NOT_NEEDED -march=armv5 -mtune=arm926ej-s
QMAKE_CXXFLAGS += -msoft-float -D__GCC_FLOAT_NOT_NEEDED -march=armv5 -mtune=arm926ej-s

include(../common/linux.conf)
include(../common/gcc-base-unix.conf)
include(../common/g++-unix.conf)

# modifications to g++.conf
QMAKE_CC                = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc -lts
QMAKE_CXX               = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-g++ -lts
QMAKE_LINK              = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-g++ -lts
QMAKE_LINK_SHLIB        = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-g++ -lts

# modifications to linux.conf
QMAKE_AR                = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-ar cqs
QMAKE_OBJCOPY           = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-objcopy
QMAKE_NM                = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-nm -P
QMAKE_STRIP             = /opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-strip

QMAKE_INCDIR += /home/bk/Qt_transplant/arm-tslib/include
QMAKE_LIBDIR += /home/bk/Qt_transplant/arm-tslib/lib
load(qt_config)
```