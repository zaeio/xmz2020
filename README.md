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
    
    mount -t nfs -o nolock 192.168.1.104:/home/nfs_share /mnt

# Qt移植
板子可能带不动qt，考虑只用tslib写界面。  
主要步骤参考`【正点原子】I.MX6U Qt移植V1.0.pdf`  ，下面是一些要修改的步骤。  
## 编译Tslib
安装automake工具  

    sudo apt-get install autoconf automake libtool  pkg-config

指定编译工具及输出路径

    ./configure --host=arm-anykav500-linux-uclibcgnueabi --cache-file=tmp.cache --prefix=/home/bk/Qt_transplant/arm-tslib CC=/opt/arm-anykav500-linux-uclibcgnueabi/usr/bin/arm-anykav500-linux-uclibcgnueabi-gcc

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

## 编译Qt
`autoconfigure.sh`配置和`qmake.conf`配置见documents  
make install后将`arm-qt`复制到nfs共享目录  
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