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
