### Compile Firmware
1. download wch riscv toolchain from its offical site(http://www.mounriver.com/download, select Linux).
2. call **make** to compile the firmware.
3. call **make flash** to upload firmware to device.(hardware requires WCH-LinkE, PD1 to SWDIO)
4. call **make test** to create the local test application.
5. run test, check result on dashboard.

note: IS31FL3731 compatible mode register address only use 1bytes, so max supported LEDs are 72 RGB LEDs(or 216 single color LEDs). Not compatible mode has two bytes for address, so max supported LEDs are 512 RGB LEDs or more(depends on the memory to buffer the LED data)

- ws2812b.is31.bin: this is compatible IS31FL3731 firmware.
- ws2812b.full.bin: this is not compatible but can use all ws2812b in line firmware.

### Link

 - Compile in Ubuntu 22.04
 - WCH CH32V003 RISCV toolchain: http://www.mounriver.com/download => Linux => MRS_Toolchain_Linux_x64_VX.XX.tar.xz
 - install libjlink0 for openocd(sudo apt install libjlink0).

