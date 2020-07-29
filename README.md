# 环境配置

## 一键编译

在/AnyCloudV500_PDK_V1.02/PDK/SDK/sdk_release_dir下执行`./auto_build.sh`  

>Type of chip : AK3760D (64M RAM)  
Type of screen : RGB  
Type of flash : spi nor flash  
Wifi card : rtl8188ftv  

可能的依赖

```
sudo apt-get install u-boot-tools
sudo apt-get install liblzo2-dev:i386 liblzma5:i386 liblzma-dev libncurses5-dev libreadline6-dev:i386 build-essential lib32stdc++6 lib32z1
```

burnfile中为编译生成的文件`uImage`、`anyka_ev500.dtb`、`u-boot.bin`、`root.sqsh4`、`usr.jffs2`、`usr.sqsh4`，用burntool烧录需要将6个文件全部复制到burntool目录下。不要用uart_burntool。  

## NFS挂载

    sudo apt-get install nfs-kernel-server
在`/etc/exports`  最后添加

    /home/bk/nfs-share *(rw,sync,no_root_squash,no_subtree_check)

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
    
    mount -t nfs -o nolock 192.168.1.104:/home/nfs_share /mnt

## Tslib移植
主要步骤参考`【正点原子】I.MX6U Qt移植V1.0.pdf`  ，下面是一些要修改的步骤。   
安装automake工具  

    sudo apt-get install autoconf automake libtool  pkg-config

编译

    ./autogen-clean.sh
    ./autogen.sh
    ./configure --host=arm-anykav500-linux-uclibcgnueabi --cache-file=tmp.cache --prefix=/home/bk/nfs-share/arm-tslib CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc
    make; make install;

make install后将`arm-tslib`复制到nfs共享目录  
在nfs目录中新建`profile`文件，然后`source /mnt/profile`，这样就不用每次修改根文件然后烧录。  
```
export T_ROOT=/mnt/arm-tslib
export LD_LIBRARY_PATH=/mnt/arm-tslib/lib:$LD_LIBRARY_PATH
export TSLIB_CONSOLEDEVICE=none
export TSLIB_FBDEVICE=/dev/fb0
export TSLIB_TSDEVICE=/dev/input/event1
export TSLIB_PLUGINDIR=$T_ROOT/lib/ts
export TSLIB_CONFFILE=$T_ROOT/etc/ts.conf
export POINTERCAL_FILE=/etc/pointercal
export TSLIB_CALIBFILE=/etc/pointercal
```

source以后，需要调用一下显示屏例程`ak_vo_sample`，然后才能运行`./mnt/arm-tslib/bin/ts_test`不知道为什么，而且触摸点左右颠倒。`ts_calibrate`由于文件系统只读暂时没效果。

编写的程序为`tests`文件夹中的`GUI.c`、`hello_ts_world.c`、`font_big.c`  
将`tests`复制到tslib中覆盖并重新编译，会生成可执行文件`hello_ts_world`、`GUI`。添加新的.c文件需要修改`Makefile.am`和`CMakeLists.txt`相应部分。

## ak_sample运行
### ak_vi_sample
用`sensor.sh`加载驱动并复制`isp_ar0230_dvp.conf`到`/etc/jffs2/`下  

    sensor.sh install ar0230
    ak_vi_sample -n 20 -f /etc/jffs2/isp_ar0230_dvp.conf

## ~~MiniGUI移植~~
到MiniGUI官网下载`libminigui-4.0.7.tar.gz`、`mg-samples-4.0.1.tar.gz`、`minigui-res-4.0.0.tar.gz`、`zlib-1.2.9.tar.gz`、`jpegsrc.v7.tar.gz`、`libpng-1.2.37.tar.gz`6个压缩包并解压。注意zlib下载1.2.9版本否则会导致系统炸裂。
### 核心库编译
先下载harfbuzz-master，将src中的.h复制到`/libminigui-4.0.7/include`中  

    git@github.com:harfbuzz/harfbuzz.git


安装依赖

    sudo apt-get install libfreetype6-dev 
进入`libminigui-3.2.3`运行

    ./configure CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc --host=arm-anykav500-linux-uclibcgnueabi --prefix=/home/bk/minigui/mg-build --build=i386-linux --target=arm-linux --disable-cursor --disable-videopcxvfb
注意--prefix=改成自己的目录；--disable-videopcxvfb必须加上  
然后`make`、`make install`

修改etc目录下的MiniGUI.cfg配置文件，首先我们要修改指定我们使用的图像引擎为fbcon然后将其分辨率信息设置我们板子对应的分辨率如下：
```
# GAL engine and default options
gal_engine=fbcon
defaultmode=800x480-16bpp
```
第二个需要配置的为输入引擎IAL这里我们使用tslib作为我们的输入引擎即如下：
```
# IAL engine
ial_engine=tslib
mdev=/dev/input/event0
mtype=IMPS2
```
还有要修改的地方就是配置正确的资源文件路径，如光标图片资源等，如下：
```
[cursorinfo]
#Edit following line to specify cursor files path
cursorpath=/share/minigui/res/cursor/

[resinfo]
respath=/share/minigui/res/
```
### 资源文件编译
进入`minigui-res-3.2.0`运行  

    ./configure --prefix=/home/bk/minigui/mg-build/; make; make install
### 依赖库编译
zlib库，注意不要加--prefix

    ./configure; make; make install
jpeg库

    ./configure --prefix=/home/bk/minigui/tpl-build CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc --host=arm-anykav500-linux-uclibcgnueabi --build=i386-linux --build=i386-linux --enable-shared --enable-static
png库

    ./configure --prefix=/home/bk//minigui/tpl-build

### sample编译

    sudo cp /minigui/mg-build/lib/pkgconfig/minigui.pc /usr/local/lib/pkgconfig
在/etc/bash.bashrc最后添加

    PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
    export PKG_CONFIG_PATH

下载mgutils  

    git clone https://github.com/VincentWei/mgutils.git
    ./autogen.sh
    ./configure CPPFLAGS=-I/home/bk/minigui/mg-build/include; make; sudo make install

编译sample，注意CPPFLAGS=-I/...改成核心库输出目录，否则error: minigui/common.h: No such file or directory  

    ./configure CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc --host=arm-anykav500-linux-uclibcgnueabi --prefix=/home/bk/minigui/mg-build --build=i386-linux --target=arm-linux CPPFLAGS=-I/home/bk/minigui/mg-build/include

## ~~Qt移植~~
板子可能带不动qt，以下可以忽略。  
`autoconfigure.sh`配置和`qmake.conf`配置见documents。make install后将`arm-qt`复制到nfs共享目录  
在`profile`后面添加
```
export QTEDIR=/mnt/arm-qt
export LD_LIBRARY_PATH=/mnt/arm-qt/lib:$QTEDIR/plugins/platforms:$LD_LIBRARY_PATH
export QT_QPA_GENERIC_PLUGINS=tslib
export QT_QPA_FONTDIR=$QTEDIR/lib/fonts 
export QT_QPA_PLATFORM_PLUGIN_PATH=$QTEDIR/plugins
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0
```
目前`arm_qt_test`界面需要很长时间才能加载出来，而且按钮也要很久才有反应，估计是性能不够。
