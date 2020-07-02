# 一键编译
在/AnyCloudV500_PDK_V1.02/PDK/SDK/sdk_release_dir下执行`./auto_build.sh`  

>Type of chip : AK3760D (64M RAM)  
Type of screen : RGB  
Type of flash : spi nor flash  
Wifi card : rtl8188ftv  

可能的依赖
>sudo apt-get install liblzo2-dev:i386  
sudo apt-get install liblzma5:i386  
sudo apt-get install liblzma-dev  
sudo apt-get install libncurses5-dev  
sudo apt-get install libx11-dev:i386 libreadline6-dev:i386  
sudo apt-get install?build-essential  
sudo apt-get install lib32stdc++6  
sudo apt-get install lib32z1  

# burnfile
编译生成的文件`uImage`、`anyka_ev500.dtb`、`u-boot.bin`、`root.sqsh4`、`usr.jffs2`、`usr.sqsh4`，用burntool烧录需要将6个文件全部复制到burntool目录下。不要用uart_burntool。  

# NFS挂载
在`/etc/exports`  最后添加
>/home/nfs_share *(rw,sync,no_root_squash,no_subtree_check)   

重启NFS  
>sudo /etc/init.d/nfs-kernel-server restart  

登陆板子终端  
username : root  
password : anycloudv500  
先运行`nfs_start.sh`，然后
>mount -t nfs -o nolock 192.168.1.104:/home/nfs_share /mnt  
