1、芯片对应关系
----------匹配符-------------------------函数接口----------支持的芯片型号--------------
    {.compatible = "hyn,66xx", .data = &cst66xx_fuc,},   /*suport 36xx、35xx、66xx、68xx */
    {.compatible = "hyn,36xxes", .data = &cst36xxes_fuc,}, /*suport 154es 3654es 3640es*/
    {.compatible = "hyn,3240", .data = &cst3240_fuc,},   /*suport 3240 */
    {.compatible = "hyn,923xx", .data = &cst923xx_fuc,},   /*suport 9217、9220 、916e、9317、317q、3217 */
    {.compatible = "hyn,3xx",  .data = &cst3xx_fuc,},    /*suport 340、348、328、128、140、148*/
    {.compatible = "hyn,7xx",  .data = &cst7xx_fuc,},    /*suport 726、826、836u*/
    {.compatible = "hyn,8xxt", .data = &cst8xxT_fuc,},   /*suport 816t、816d、820、08C*/
    {.compatible = "hyn,226se", .data = &cst226se_fuc,}, /*suport 226se 8922*/
    {.compatible = "hyn,840u", .data = &cst840u_fuc,},   /*suport 840u*/
    {.compatible = "hyn,76xx", .data = &cst76xx_fuc,},   /*suport 7864BG 7964BG HYT7864JL HYT7760BG HYT7760TR CST6960BG*/


2、参考dts配置 下面都是必配项:
&i2c1{
	#address-cells = <1>;
	#size-cells = <0>;
	status = "okay";
	hynitron@5A {
		compatible = "hyn,66xx";   //根据芯片型号配置
		reg = <0x5A>;			   
		pos-swap = <0>;     	   //xy坐标交换
		posx-reverse = <0>; 	   //x坐标反向
		posy-reverse = <0>;        //y坐标反向
	};
};


3、参考Makefile配置:
obj-y += hynitron_touch.o
hynitron_touch-objs += hyn_core.o hyn_lib/hyn_i2c.o hyn_lib/hyn_spi.o hyn_lib/hyn_ts_ext.o hyn_lib/hyn_fs_node.o
hynitron_touch-objs += hyn_lib/hyn_tool.o
hynitron_touch-objs += hyn_lib/hyn_gesture.o
hynitron_touch-objs += hyn_lib/hyn_prox.o
hynitron_touch-objs += hyn_chips/hyn_cst66xx.o
hynitron_touch-objs += hyn_chips/hyn_cst3240.o
hynitron_touch-objs += hyn_chips/hyn_cst92xx.o
hynitron_touch-objs += hyn_chips/hyn_cst3xx.o
hynitron_touch-objs += hyn_chips/hyn_cst1xx.o  
hynitron_touch-objs += hyn_chips/hyn_cst7xx.o
hynitron_touch-objs += hyn_chips/hyn_cst8xxT.o
hynitron_touch-objs += hyn_chips/hyn_cst7xx.o
hynitron_touch-objs += hyn_chips/hyn_cst840u.o


4、sys节点操作
1、升级
    通过文件升级
    adb push xxx.bin /sdcard/app.bin
    adb shell "cd /sys/devices/platform/xxxx/i2c-7/7-005a echo fd>./hyntpdbg && cat ./hyntpfwver"
    通过dump升级(GKI version)
    adb root
    adb push xxx.bin /sdcard/app.bin
    adb shell "cd /sys/devices/platform/soc/xxxx/i2c-7/7-005a && echo fwstart>./hyndumpfw && dd if=/sdcard/app.bin of=./hyndumpfw && echo fwend>./hyndumpfw"
    
2、write 
    eg:写 d1 01 02 03 04
    echo w d1 01 02 03 04 >/sys/hynitron_debug/hyntpdbg
3、read
    eg 读 20 byte
    echo r 20 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
3、read reg （max reg长度4 byte max read 256 byte）
    eg:写 d1 01 读 2 byte
    echo w d1 01 r 2 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
    eg:写 d1 01 02 03 读 20 byte
    echo w d1 01 02 03 r 20 >/sys/hynitron_debug/hyntpdbg && cat /sys/hynitron_debug/hyntpdbg
    如果reg 不变可以直接用 cat /sys/hynitron_debug/hyntpdbg 读（reg沿用上次的操作）
4、调试log debug
    echo 7>/proc/sys/kernel/printk
    echo log,3>/sys/hynitron_debug/hyntpdbg

5、读版TP_FW本号
    cat /sys/hynitron_debug/hyntpfwver

6、tp0 自测(需要提前准备自测配置文件)
    cat /sys/hynitron_debug/hynselftest

7、充电模式进入和退出
    enter：
    echo c1>/sys/hynitron_debug/hynswitchmode
    exit：
    echo c0>/sys/hynitron_debug/hynswitchmode
8、手套模式进入和退出
    enter：
    echo g1>/sys/hynitron_debug/hynswitchmode
    exit：
    echo g0>/sys/hynitron_debug/hynswitchmode





