# **Berlin Driver Porting Guide**

## **Introduce**

* Support I2C or SPI connection.
* Support multi devices online at the same time.
* List of supported chips:
  * BerlinA(GT9897)
  * BerlinB(GT9966)
  * BerlinD(GT9916)
  * Nottingham(GT9895)
  * Marseille(GT9615)
  * AtlantaB(GT9926)
  * SanJoseC(GT9976)

## **Driver source file prepare**

1. Move driver source code to `$KERNEL_SRC/drivers/input/touchscree/`
2. Change `$KERNEL_SRC/drivers/input/touchscree/Makefile` 

    Add this line to the Makefile

    ```makefile
    obj-y += goodix_berlin_driver/
    ```

3. Change `$KERNEL_SRC/drivers/input/touchscree/Kconfg`

    Add this line to the Kconfig

    ```conf
    source "drivers/input/touchscreen/goodix_berlin_driver/Kconfig"
    ```

## **Add device declaration in the board devicetree**

Please add Goodix touch device declaration info in the board devicetree, you can refer the Appendix goodix-ts-i2c-dtsi or goodix-ts-spi-dtsi  to see how to set deivce properties.

## **Build driver**

When build kernel you will see the following promt to let you confirm how to build the driver. This driver support built-in kernel or build as modules.

**Setting up in the menu:**

1. In the $KERNEL_SRC directory, exec `make menuconfig`, then select  
`Device Drivers ---> Input device support ---> Touchscreens --->`  
2. Find `Goodix berlin touchscreen` menu, you can select `<*>`(build in kernel) or `<M>`(build a module).
3. Enter `Goodix berlin touchscreen`, you can see `support SPI bus connection` item. If
you are on SPI connection, select `<*>`, or on I2C connection.

**Setting up in the defconfig file:**

1. Add the following in you defconfig file.

 ```conf
 CONFIG_TOUCHSCREEN_GOODIX_BRL=y
 ```

 or

 ```conf
 CONFIG_TOUCHSCREEN_GOODIX_BRL=m
 ```

2. If you are on SPI connection, add the following.

 ```conf
 CONFIG_TOUCHSCREEN_GOODIX_BRL_SPI=y
 ```

3. If you are on I2C connection, add the following.

 ```conf
 CONFIG_TOUCHSCREEN_GOODIX_BRL_I2C=y
 ```

## **Appendix**

### **I2C DTS**

```dts
goodix-berlin@5d {
  /*
    compatible name:
    9897 select goodix,brl-a
    9966 select goodix,brl-b
    9916 select goodix,brl-d
    9895 select goodix,nottingham
    9615 select goodix,marseille
    9926 select goodix,atb
    9976 select goodix,brl-b
  */
  compatible = "goodix,brl-a";
  goodix,avdd-name = "avdd";
  avdd-supply = <&pm8916_l15>;
  goodix,iovdd-name = "iovdd";
  iovdd-supply = <&pm8916_l16>;

  reg = <0x5d>; /* i2c slave addr */
  goodix,reset-gpio = <&msm_gpio 12 0x0>;
  goodix,irq-gpio = <&msm_gpio 13 0x0>;
  goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling;*/
  goodix,panel-max-x = <720>;
  goodix,panel-max-y = <1280>;
  goodix,panel-max-w = <255>;

  /* optional properties */
  goodix,panel-max-p = <4096>; /* max pressure that pen device supported */
  goodix,pen-enable; /* support active stylus device */
  goodix,sleep-enable; /* enter sleep mode when screen off */
  goodix,esd-enable;  /* enable esd function */
  goodix,firmware-name = "goodix_firmware.bin"; /* set firmware name */
  goodix,config-name = "goodix_cfg_group.bin"; /* set config name */
};
```

### **SPI DTS**

```dts
goodix-berlin@0 {
  /*
    compatible name:
    9897 select goodix,brl-a
    9966 select goodix,brl-b
    9916 select goodix,brl-d
    9895 select goodix,nottingham
    9615 select goodix,marseille
    9926 select goodix,atb
    9976 select goodix,brl-b
  */
  compatible = "goodix,brl-a";
  reg = <0>;
  spi-max-frequency = <2000000>;

  goodix,avdd-name = "avdd";
  avdd-supply = <&pm8916_l15>;
  goodix,iovdd-name = "iovdd";
  iovdd-supply = <&pm8916_l16>;

  goodix,reset-gpio = <&msm_gpio 12 0x0>;
  goodix,irq-gpio = <&msm_gpio 13 0x0>;
  goodix,irq-flags = <2>; /* 1:trigger rising, 2:trigger falling; */
  goodix,panel-max-x = <720>;
  goodix,panel-max-y = <1280>;
  goodix,panel-max-w = <256>;

  /* optional properties */
  goodix,panel-max-p = <4096>; /* max pressure that pen device supported */
  goodix,pen-enable; /* support active stylus device */
  goodix,sleep-enable; /* enter sleep mode when screen off */
  goodix,esd-enable;  /* enable esd function */
  goodix,firmware-name = "goodix_firmware.bin"; /* set firmware name */
  goodix,config-name = "goodix_cfg_group.bin"; /* set config name */
};
```
