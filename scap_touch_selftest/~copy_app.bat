adb root
adb push selftest  /data/
adb push factory.txt  /data/
adb shell setenforce 0
adb shell chmod 777 /data/selftest
pause