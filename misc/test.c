// gcc i2c.c -o i2c -lusb-1.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define MAX_I2C_PACK 16         // V7B: 16, MPRO: 59
#ifndef min
#define min(a, b)    ((a) < (b) ? (a) : (b))
#endif

#define MAX_RGB_LED    24

static libusb_context *context = NULL;
static libusb_device_handle *handle = NULL;

int v2s_i2c_read_reg16(uint8_t addr, uint16_t reg, uint8_t *d, uint8_t size)
{
    uint8_t buf[64] = {0};
    int r;

    if (size > MAX_I2C_PACK)
        return -1;         // required data is too much.

    buf[0] = addr;         // i2c device address
    buf[1] = sizeof(reg);  // write 2 bytes for register.
    buf[2] = size;         // read data size.
    buf[3] = reg >> 8;     // i2c device register high byte.
    buf[4] = reg & 0xff;   // i2c device register low byte.

    // send data to device i2c buffer.
    r = libusb_control_transfer(handle, 0x40, 0xb5, 0, 0, buf, 5, 200);
    if (r < 0)
        return r;

    // trigger write to device, must send same device address.
    r = libusb_control_transfer(handle, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    if (r < 0)
        return r;

    // trigger read from device, first byte need to be the address.
    r = libusb_control_transfer(handle, 0xc0, 0xb7, 0, 0, buf, size + 1, 200);
    if (r < 0)
        return r;

    memcpy(d, buf + 1, size);
    return size;
}

int v2s_i2c_write_reg16(uint8_t addr, uint16_t reg, uint8_t *d, uint8_t size)
{
    uint8_t buf[64] = {0};
    int r;

    if (size > MAX_I2C_PACK)
        return -1;         // required data is too much.

    buf[0] = addr;
    buf[1] = sizeof(reg) + size;  // write 2 bytes for register and rest for data.
    buf[2] = 0;            // read data size.
    buf[3] = reg >> 8;     // i2c device register high byte.
    buf[4] = reg & 0xff;   // i2c device register low byte.

    memcpy(buf + 5, d, size);

    // send data to device i2c buffer.
    r = libusb_control_transfer(handle, 0x40, 0xb5, 0, 0, buf, 5 + size, 200);
    if (r < 0)
        return r;

    // trigger write to device, first byte need to be the address.
    r = libusb_control_transfer(handle, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    if (r < 0)
        return r;

    return size;
}

int v2s_i2c_write_reg16s(uint8_t addr, uint16_t reg, uint8_t *d, uint8_t size)
{
    int used = 0;
    while (used < size) {
        int cur_size = min(size - used, MAX_I2C_PACK);
        v2s_i2c_write_reg16(addr, reg + used, d + used, cur_size);
        used += cur_size;
    }
}

int v2s_i2c_read_reg8(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t size)
{
    uint8_t buf[64] = {0};
    int r;

    if (size > MAX_I2C_PACK)
        return -1;         // required data is too much.

    buf[0] = addr;         // i2c device address
    buf[1] = sizeof(reg);  // write 2 bytes for register.
    buf[2] = size;         // read data size.
    buf[3] = reg & 0xff;   // i2c device register low byte.

    // send data to device i2c buffer.
    r = libusb_control_transfer(handle, 0x40, 0xb5, 0, 0, buf, 4, 200);
    if (r < 0)
        return r;

    // trigger write to device, must send same device address.
    r = libusb_control_transfer(handle, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    if (r < 0)
        return r;

    // trigger read from device, first byte need to be the address.
    r = libusb_control_transfer(handle, 0xc0, 0xb7, 0, 0, buf, size + 1, 200);
    if (r < 0)
        return r;

    memcpy(d, buf + 1, size);
    return size;
}

int v2s_i2c_write_reg8(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t size)
{
    uint8_t buf[64] = {0};
    int r;

    if (size > MAX_I2C_PACK)
        return -1;         // required data is too much.

    buf[0] = addr;
    buf[1] = sizeof(reg) + size;  // write 2 bytes for register and rest for data.
    buf[2] = 0;            // read data size.
    buf[3] = reg & 0xff;   // i2c device register low byte.

    memcpy(buf + 4, d, size);

    // send data to device i2c buffer.
    r = libusb_control_transfer(handle, 0x40, 0xb5, 0, 0, buf, 4 + size, 200);
    if (r < 0)
        return r;

    // trigger write to device, first byte need to be the address.
    r = libusb_control_transfer(handle, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    if (r < 0)
        return r;

    return size;
}

int v2s_i2c_write_reg8s(uint8_t addr, uint8_t reg, uint8_t *d, uint8_t size)
{
    int used = 0;
    while (used < size) {
        int cur_size = min(size - used, MAX_I2C_PACK);
        v2s_i2c_write_reg8(addr, reg + used, d + used, cur_size);
        used += cur_size;
    }
}

int v2s_i2c_write_reg8_byte(uint8_t addr, uint8_t reg, uint8_t d)
{
    return v2s_i2c_write_reg8(addr, reg, &d, 1);
}

int get_screen(libusb_device_handle *h)
{
    unsigned char buf[5] = {0x51, 0x02, 0x04, 0x1f, 0xfc};
    libusb_control_transfer(h, 0x40, 0xb5, 0, 0, buf, 5, 200);
    libusb_control_transfer(h, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    libusb_control_transfer(h, 0xc0, 0xb7, 0, 0, buf, 5, 200);
    return ((int *)(buf + 1))[0];
}

int get_version(libusb_device_handle *h)
{
    unsigned char buf[5] = {0x51, 0x02, 0x04, 0x1f, 0xf8};
    libusb_control_transfer(h, 0x40, 0xb5, 0, 0, buf, 5, 200);
    libusb_control_transfer(h, 0xc0, 0xb6, 0, 0, buf, 1, 200);
    libusb_control_transfer(h, 0xc0, 0xb7, 0, 0, buf, 5, 200);
    return ((int *)(buf + 1))[0];
}

void get_screen_id(libusb_device_handle *h, unsigned char *id)
{
    unsigned char cmd[] = {0x51, 0x02, 0x08, 0x1f, 0xf0};
    unsigned char buf[9] = {0};

    libusb_control_transfer(h, 0x40, 0xb5, 0, 0, cmd, 5, 200);
    libusb_control_transfer(h, 0xc0, 0xb6, 0, 0, cmd, 1, 200);
    libusb_control_transfer(h, 0xc0, 0xb7, 0, 0, buf, 9, 200);
    memcpy(id, buf + 1, 8);
}

void print_screen_id(libusb_device_handle *h)
{
    unsigned char id[8] = {0};
    get_screen_id(h, id);
    printf("%02x%02x-%02x%02x-%02x%02x%02x%02x\n",
           id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
}

void get_screen_version(libusb_device_handle *h, int *frame_size, int *margin,
                        int *pos, char *buf)
{
    uint32_t ver = get_screen(h);
    uint32_t code = get_version(h);

    switch (ver) {
    case 0x00000005:
        if (buf) {
            if (code == 0xffffffff)
                sprintf(buf, "5inch, 480x854x16/24, SLM5.0-81FPC-A");
            if (code == 0x00000000)
                sprintf(buf, "5inch, 480x854x16/24, D500FPC9373-C");
            if (code == 0x00000003) {
                sprintf(buf, "5inch, 480x854x16/24, D500FPC931A-A");
            }
        }
        if (frame_size)
            *frame_size = 854 *480 * 2;
        if (margin) {
            if(code != 0x00000003)
                *margin = 320;
            else
                *margin = 0;
        }
        if (pos)
            *pos = 0;
        break;

    case 0x00000304:
        if (buf)
            sprintf(buf, "4.3inch, 480x800x16/24, D430FPC9316-A");
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 0;
        break;

    case 0x00000004:
        if (buf) {
            if (code == 0x00000002)
                sprintf(buf, "4inch, 480x800x16/24, VOCORE-4NDNV7B");
            else
                sprintf(buf, "4inch, 480x800x16, TOSHIBA-2122");
        }
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 0;
        break;

    case 0x00000b04:
        if (buf && code == 0xffffffff)
            sprintf(buf, "4inch, 480x800x16/24, D397FPC9367-B");
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 0;
        break;

    case 0x00000104:
        if (buf) {
            if (code == 0xffffffff)
                sprintf(buf, "4inch, 480x800x16/24, DJN-1922");
            else if (code == 0x00000001)
                sprintf(buf, "4inch, 480x800x16/24, SLM4.0-33FPC-A");
            else if (code == 0x00000002)
                sprintf(buf, "4inch, 480x800x16/24, VOCORE-4NDNV10B");
        }
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 0;
        break;

    case 0x00000007:
        if (buf)
            sprintf(buf, "6.8inch, 800x480x16/24, D680FPC930G-A");
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 1;
        break;

    default:
        if (buf)
            sprintf(buf, "4inch/5inch, 480x800x16, code = %08x", ver);
        if (frame_size)
            *frame_size = 800 *480 * 2;
        if (margin)
            *margin = 0;
        if (pos)
            *pos = 0;
        break;
    }
}

int get_port_handle(uint16_t vid, uint16_t pid, libusb_device_handle *handles[], int size)
{
    struct libusb_device **list = NULL;
    int used = 0;

    int count = libusb_get_device_list(context, &list);
    for (int i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;

        memset(&desc, 0, sizeof(struct libusb_device_descriptor));
        if (libusb_get_device_descriptor(list[i], &desc))
            continue;

        int curport = libusb_get_port_number(list[i]) - 1;
        if (desc.idVendor == vid && desc.idProduct == pid) {
            char screen_type[0x100] = {0};

            libusb_open(list[i], &handles[used]);

            libusb_claim_interface(handles[used], 0);
            //libusb_set_interface_alt_setting(handles[used], 0, 0);

            get_screen_version(handles[used], NULL, NULL, NULL, screen_type);

            printf("Find screen at USB physics port %d\n", curport);
            printf("Type: %s\n", screen_type);
            printf("Screen UID: "); print_screen_id(handles[used]);

            used++;
            if (used >= size) {
                printf("reach max allowed devices number for this test.\n");
                break;
            }
        }
    }

    libusb_free_device_list(list, 0);
    return used;
}

int main(int argc, char *argv[])
{
    libusb_device_handle *handles[0x10] = {NULL};

    if (argc > 3) {
        printf("i2c_test [r/w] [data]\n");
        printf("show frame: screen_test frame.dat\n");
        printf("adjust brightness: screen_test b50\n");
        return 0;
    }

    printf("Version: %s,%s\n\n", __DATE__, __TIME__);

    // 1. find and open usb device.
    printf("Scaning for VoCore screen...\n\n");

    libusb_init(&context);
    int device_count = get_port_handle(0xc872, 0x1004, handles, 0x10);
    if (device_count == 0) {
        printf("No screen find, please check driver or USB cable.\n"
               "Press any key to exit.\n");
        getchar();      // avoid suddenly exit for windows.
        return -1;
    }

    handle = handles[0];
    printf("Detected %d screen(s), use first one for test.\n", device_count);

    // close rest screen handles.
    for (int i = 1; i < device_count; i++) {
        if (handles[i]) {
            libusb_release_interface(handles[i], 0);
            libusb_close(handles[i]);
        }
    }

    int count = 0, color = 0;

#ifdef IS31FL3731_COMPATIBLE
    // MPRO firmware >= v0.25 support gpio driver WS2812B.
    // note: IS31FL3731 init code is not necessary for MPRO GPIO or I2C mode.
    uint8_t c = 1;
    libusb_control_transfer(handle, 0x40, 0xbf, 0, 0, &c, 1, 200);

    // IS31FL3731 init code.
    // read chip id to check if the connect is all right.
    v2s_i2c_write_reg8_byte(0x74, 0xfd, 0x0b);
    v2s_i2c_write_reg8_byte(0x74, 0x0a, 0x00);
    usleep(10000);
    v2s_i2c_write_reg8_byte(0x74, 0x0a, 0x01);   // restore
    v2s_i2c_write_reg8_byte(0x74, 0x00, 0x01);   // picture mode
    v2s_i2c_write_reg8_byte(0x74, 0x01, 0x00);   // picture use bank 0

    // clear all LEDs and registers.
    v2s_i2c_write_reg8_byte(0x74, 0xfd, 0);
    for (uint32_t i = 0; i <= 0xb4; i += 0x10) {
        uint8_t reg[0x10] = {0};
        v2s_i2c_write_reg8(0x74, i, reg, sizeof(reg));
    }

    // enable all leds.
    uint8_t reg[0x12];
    memset(reg, 0xff, 0x12);
    v2s_i2c_write_reg8(0x74, 0, reg, 0x12);
    // IS31FL3731 init code end here.

    while (1) {
            uint8_t d[MAX_RGB_LED * 3] = {0};

            for (int color = 0; color < 3; color++) {
                for (int count = 0; count < MAX_RGB_LED; count++) {
                    memset(d, 0, sizeof(d));
                    d[color + count * 3] = 0xff;
                    v2s_i2c_write_reg8s(0x74, 0x24, d, sizeof(d));
                    usleep(100000);
                }
            }

            for (int color = 0; color < 3; color++) {
                for (int count = MAX_RGB_LED - 1; count >= 0; count--) {
                    memset(d, 0, sizeof(d));
                    d[color + count * 3] = 0xff;
                    v2s_i2c_write_reg8s(0x74, 0x24, d, sizeof(d));
                    usleep(100000);
                }
            }

            memset(d, 0, sizeof(d));
            for (int count = 0; count < MAX_RGB_LED; count++) {
                d[count * 3] = 0xff;
            }
            v2s_i2c_write_reg8s(0x74, 0x24, d, sizeof(d));
            sleep(1);

            memset(d, 0, sizeof(d));
            for (int count = 0; count < MAX_RGB_LED; count++) {
                d[count * 3 + 1] = 0xff;
            }
            v2s_i2c_write_reg8s(0x74, 0x24, d, sizeof(d));
            sleep(1);

            memset(d, 0, sizeof(d));
            for (int count = 0; count < MAX_RGB_LED; count++) {
                d[count * 3 + 2] = 0xff;
            }
            v2s_i2c_write_reg8s(0x74, 0x24, d, sizeof(d));
            sleep(1);
    }


#else // default 512 LEDs mode.

    while (1) {
        uint8_t d[MAX_RGB_LED * 3] = {0};

        d[color + count * 3] = 0x10;
        v2s_i2c_write_reg16s(0x74, 0, d, sizeof(d));

        count++;
        if (count >= sizeof(d) / 3) {
            count = 0;
            color++;
            color %= 3;
        }

        usleep(10000);
    }

#endif

end:
    // close usb port.
    libusb_release_interface(handle, 0);
    libusb_close(handle);
    libusb_exit(context);
    return 0;
}
