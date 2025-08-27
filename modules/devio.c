/* quadcastrgb - set RGB lights of HyperX Quadcast S and DuoCast
 * File devio.c
 *
 * <----- License notice ----->
 * Copyright (C) 2022, 2023, 2024 Ors1mer
 *
 * You may contact the author by email:
 * ors1mer [[at]] ors1mer dot xyz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License ONLY.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see
 * <https://www.gnu.org/licenses/gpl-2.0.en.html>. For any questions
 * concerning the license, you can write to <licensing@fsf.org>.
 * Also, you may visit the Free Software Foundation at
 * 51 Franklin Street, Fifth Floor Boston, MA 02110 USA.
 */
#include <unistd.h> /* for usleep */
#include <fcntl.h> /* for daemonization */
#include <signal.h> /* for signal handling */

#include "locale_macros.h"

#include "devio.h"

/* Constants */

#define DEV_EPOUT 0x00 /* control endpoint OUT */
#define DEV_EPIN 0x80 /* control endpoint IN */
/* Packet info */
#define MAX_PCT_CNT 90
#define PACKET_SIZE 64 /* bytes */

#define HEADER_CODE 0x04
#define DISPLAY_CODE 0xf2
#define PACKET_CNT 0x01

#define INTR_EP_IN 0x82
#define INTR_LENGTH 8

#define TIMEOUT 1000 /* one second per packet */
#define BMREQUEST_TYPE_OUT 0x21
#define BREQUEST_OUT 0x09
#define BMREQUEST_TYPE_IN 0xa1
#define BREQUEST_IN 0x01
#define WVALUE 0x0300
#define WINDEX 0x0000
/* Messages */
#define DEVLIST_ERR_MSG _("Couldn't get the list of USB devices.\n")
#define NODEV_ERR_MSG _("HyperX Quadcast S/DuoCast isn't connected or accessible through USB.\n")
#define OPEN_ERR_MSG _("Couldn't open the microphone.\n")
#define BUSY_ERR_MSG _("Another program is using the microphone already. " \
                       "Stopping.\n")
#define TRANSFER_ERR_MSG _("Couldn't transfer a packet! " \
                           "The device might be busy.\n")
#define FOOTER_ERR_MSG _("Footer packet error: %s\n")
#define HEADER_ERR_MSG _("Header packet error: %s\n")
#define SIZEPCK_ERR_MSG _("Size packet error: %s\n")
#define DATAPCK_ERR_MSG _("Data packet error: %s\n")
#define PID_MSG _("Started with pid %d\n")
/* Error codes */
enum {
    libusberr = 2,
    nodeverr,
    devopenerr,
    transfererr
};

/* For open_micro */
#define FREE_AND_EXIT() \
    libusb_free_device_list(devs, 1); \
    free(data_arr); \
    libusb_exit(NULL); \
    exit(libusberr)

#define HANDLE_ERR(CONDITION, MSG) \
    if(CONDITION) { \
        fprintf(stderr, MSG); \
        FREE_AND_EXIT(); \
    }
/* For send_packets */
#define HANDLE_TRANSFER_ERR(ERRCODE) \
    if(ERRCODE) { \
        fprintf(stderr, TRANSFER_ERR_MSG); \
        libusb_close(handle); \
        libusb_exit(NULL); \
        free(data_arr); \
        exit(transfererr); \
    }

/* Vendor IDs */
#define DEV_VID_KINGSTON      0x0951
#define DEV_VID_HP            0x03f0
/* Product IDs */
const unsigned short product_ids_kingston[] = {
    0x171f
};
const unsigned short product_ids_hp[] = {
    0x0f8b,
    0x028c,
    0x048c,
    0x068c,
    0x098c  /* Duocast */
};

/* Microphone opening */
static int claim_dev_interface(libusb_device_handle *handle);
static libusb_device *dev_search(libusb_device **devs, ssize_t cnt);
static int is_compatible_mic(libusb_device *dev);
/* Packet transfer */
static short send_display_command(byte_t *packet,
                                  libusb_device_handle *handle);
static int display_data_arr(libusb_device_handle *handle,
                             const byte_t *colcommand, const byte_t *end);
#if !defined(DEBUG) && !defined(OS_MAC)
static void daemonize(int verbose);
#endif
#ifdef DEBUG
static void print_packet(byte_t *pck, char *str);
#endif

/* Signal handling */
volatile static sig_atomic_t nonstop = 0; /* BE CAREFUL: GLOBAL VARIABLE */
static void nonstop_reset_handler(int s)
{
    /* No need in saving errno or setting the handler again
     * because the program just frees memory and exits */
    nonstop = 0;
}

/* Functions */
libusb_device_handle *open_micro(datpack *data_arr)
{
    libusb_device **devs;
    libusb_device *micro_dev = NULL;
    libusb_device_handle *handle;
    ssize_t dev_count;
    short errcode;
    int retry_count;

    errcode = libusb_init(NULL);
    if(errcode) {
        perror("libusb_init");
        free(data_arr); exit(libusberr);
    }

    /* Set libusb options for better USB hub compatibility */
    #if LIBUSB_API_VERSION >= 0x01000106
    libusb_set_option(NULL, LIBUSB_OPTION_WEAK_AUTHORITY);
    #endif

    /* Try multiple times with delays for USB hub enumeration */
    for(retry_count = 0; retry_count < 3; retry_count++) {
        if(retry_count > 0) {
            #ifdef DEBUG
            printf("Retry attempt %d after delay...\n", retry_count + 1);
            #endif
            usleep(500000); /* 500ms delay */
        }

        dev_count = libusb_get_device_list(NULL, &devs);
        if(dev_count < 0) {
            if(retry_count < 2) {
                libusb_free_device_list(devs, 1);
                continue;
            }
            fprintf(stderr, DEVLIST_ERR_MSG);
            FREE_AND_EXIT();
        }

        #ifdef DEBUG
        printf("Found %zd USB devices\n", dev_count);
        #endif

        micro_dev = dev_search(devs, dev_count);
        if(micro_dev) {
            #ifdef DEBUG
            printf("Compatible device found!\n");
            #endif
            break;
        }

        libusb_free_device_list(devs, 1);
    }

    HANDLE_ERR(!micro_dev, NODEV_ERR_MSG);
    /* Try opening device with retries for USB hub issues */
    for(retry_count = 0; retry_count < 3; retry_count++) {
        if(retry_count > 0) {
            #ifdef DEBUG
            printf("Retrying device open (attempt %d)...\n", retry_count + 1);
            #endif
            usleep(200000); /* 200ms delay */
        }

        errcode = libusb_open(micro_dev, &handle);
        if(errcode == 0) {
            #ifdef DEBUG
            printf("Device opened successfully\n");
            #endif
            break;
        }

        #ifdef DEBUG
        printf("Open failed: %s\n", libusb_strerror(errcode));
        #endif
    }

    if(errcode) {
        fprintf(stderr, "%s\n%s", libusb_strerror(errcode), OPEN_ERR_MSG);
        FREE_AND_EXIT();
    }
    errcode = claim_dev_interface(handle);
    if(errcode) {
        libusb_close(handle); FREE_AND_EXIT();
    }
    libusb_free_device_list(devs, 1);
    return handle;
}

static int claim_dev_interface(libusb_device_handle *handle)
{
    int errcode0, errcode1;
    int retry;

    libusb_set_auto_detach_kernel_driver(handle, 1); /* might be unsupported */

    /* Try to claim interfaces with retries for USB hub timing issues */
    for(retry = 0; retry < 3; retry++) {
        if(retry > 0) {
            #ifdef DEBUG
            printf("Retrying interface claim (attempt %d)...\n", retry + 1);
            #endif
            usleep(100000); /* 100ms delay */
        }

        errcode0 = libusb_claim_interface(handle, 0);
        errcode1 = libusb_claim_interface(handle, 1);

        if(errcode0 == 0 && errcode1 == 0) {
            #ifdef DEBUG
            printf("Successfully claimed both interfaces\n");
            #endif
            return 0;
        }

        /* Release any claimed interfaces before retry */
        if(errcode0 == 0) libusb_release_interface(handle, 0);
        if(errcode1 == 0) libusb_release_interface(handle, 1);
    }

    /* Final error handling */
    if(errcode0 == LIBUSB_ERROR_BUSY || errcode1 == LIBUSB_ERROR_BUSY) {
        fprintf(stderr, BUSY_ERR_MSG);
        return 1;
    } else if(errcode0 == LIBUSB_ERROR_NO_DEVICE ||
                                          errcode1 == LIBUSB_ERROR_NO_DEVICE) {
        fprintf(stderr, OPEN_ERR_MSG);
        return 1;
    }

    #ifdef DEBUG
    printf("Interface claim failed: if0=%s, if1=%s\n",
           libusb_strerror(errcode0), libusb_strerror(errcode1));
    #endif

    return 1;
}

static libusb_device *dev_search(libusb_device **devs, ssize_t cnt)
{
    libusb_device **dev;
    #ifdef DEBUG
    printf("Searching through %zd devices...\n", cnt);
    #endif

    for(dev = devs; dev < devs+cnt; dev++) {
        /* Small delay between device checks for hub compatibility */
        usleep(10000); /* 10ms */

        if(is_compatible_mic(*dev))
            return *dev;
    }
    return NULL;
}

static int is_compatible_mic(libusb_device *dev)
{
    int i, arr_size, ret;
    const unsigned short *product_id_arr;
    struct libusb_device_descriptor descr;

    ret = libusb_get_device_descriptor(dev, &descr);
    if(ret < 0) {
        #ifdef DEBUG
        printf("Failed to get device descriptor: %s\n", libusb_strerror(ret));
        #endif
        return 0;
    }

    #ifdef DEBUG
    printf("Checking device: Vendor=%04x Product=%04x\n", descr.idVendor, descr.idProduct);
    #endif

    if (descr.idVendor == DEV_VID_KINGSTON) {
        product_id_arr = product_ids_kingston;
        arr_size = sizeof(product_ids_kingston)/sizeof(*product_id_arr);
    } else if (descr.idVendor == DEV_VID_HP) {
        product_id_arr = product_ids_hp;
        arr_size = sizeof(product_ids_hp)/sizeof(*product_id_arr);
    } else {
        return 0;
    }

    #ifdef DEBUG
    printf("Valid vendor found: %04x\nTrying product ids:\n", descr.idVendor);
    #endif
    for (i = 0; i < arr_size; i++) {
        #ifdef DEBUG
        printf("\t%04x\n", product_id_arr[i]);
        #endif
        if (descr.idProduct == product_id_arr[i])
            return 1;
    }
    return 0;
}

static libusb_device_handle *attempt_reconnect(void);

void send_packets(libusb_device_handle *handle, const datpack *data_arr,
                  int pck_cnt, int verbose)
{
    short command_cnt;
    int reconnect_attempts = 0;
    libusb_device_handle *current_handle = handle;
    #ifdef DEBUG
    puts("Entering display mode...");
    #endif
    #if !defined(DEBUG) && !defined(OS_MAC)
    daemonize(verbose);
    #endif
    command_cnt = count_color_commands(data_arr, pck_cnt, 0);
    signal(SIGINT, nonstop_reset_handler);
    signal(SIGTERM, nonstop_reset_handler);
    /* The loop works until a signal handler resets the variable */
    nonstop = 1; /* set to 1 only here */
    while(nonstop) {
        int display_result = display_data_arr(current_handle, *data_arr, *data_arr+2*BYTE_STEP*command_cnt);
        if(display_result != 0 && nonstop) {
            /* USB error occurred, try to reconnect */
            #ifdef DEBUG
            puts("USB error detected, attempting to reconnect...");
            #endif
            if(current_handle) {
                libusb_release_interface(current_handle, 0);
                libusb_release_interface(current_handle, 1);
                libusb_close(current_handle);
                current_handle = NULL;
            }

            /* Wait before reconnecting */
            usleep(1000000); /* 1 second delay */

            /* Try to reconnect */
            while(nonstop && current_handle == NULL) {
                libusb_device_handle *new_handle = attempt_reconnect();
                if(new_handle != NULL) {
                    current_handle = new_handle;
                    reconnect_attempts = 0;
                    #ifdef DEBUG
                    puts("Successfully reconnected to device!");
                    #endif
                } else {
                    reconnect_attempts++;
                    #ifdef DEBUG
                    printf("Reconnection attempt %d failed, waiting...\n", reconnect_attempts);
                    #endif
                    usleep(2000000); /* 2 second delay between attempts */
                }
            }
        }
    }

    /* Clean up when exiting */
    if(current_handle) {
        libusb_release_interface(current_handle, 0);
        libusb_release_interface(current_handle, 1);
        libusb_close(current_handle);
    }
}

#if !defined(DEBUG) && !defined(OS_MAC)
static void daemonize(int verbose)
{
    int pid;

    chdir("/");
    pid = fork();
    if(pid > 0)
        exit(0);
    setsid();
    pid = fork();
    if(pid > 0)
        exit(0);

    if(verbose)
        printf(PID_MSG, getpid()); /* notify the user */
    fflush(stdout); /* force clear of the buffer */
    close(0);
    close(1);
    close(2);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}
#endif

static int display_data_arr(libusb_device_handle *handle,
                             const byte_t *colcommand, const byte_t *end)
{
    short sent;
    byte_t *packet;
    byte_t header_packet[PACKET_SIZE] = {
        HEADER_CODE, DISPLAY_CODE, 0, 0, 0, 0, 0, 0, PACKET_CNT, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    packet = calloc(PACKET_SIZE, 1);
    while(colcommand < end && nonstop) {
        sent = send_display_command(header_packet, handle);
        if(sent != PACKET_SIZE) {
            free(packet);
            return -1; /* Return error instead of setting nonstop */
        }
        memcpy(packet, colcommand, 2*BYTE_STEP);
        sent = libusb_control_transfer(handle, BMREQUEST_TYPE_OUT,
                   BREQUEST_OUT, WVALUE, WINDEX, packet, PACKET_SIZE, TIMEOUT);
        if(sent != PACKET_SIZE) {
            free(packet);
            return -1; /* Return error instead of setting nonstop */
        }
        #ifdef DEBUG
        print_packet(packet, "Data:");
        #endif
        colcommand += 2*BYTE_STEP;
        usleep(1000*20);  /* Reduced from 55ms to 20ms for faster color updates */
    }
    free(packet);
    return 0; /* Success */
}

static short send_display_command(byte_t *packet, libusb_device_handle *handle)
{
    short sent;
    sent = libusb_control_transfer(handle, BMREQUEST_TYPE_OUT, BREQUEST_OUT,
                                 WVALUE, WINDEX, packet, PACKET_SIZE,
                                 TIMEOUT);
    #ifdef DEBUG
    print_packet(packet, "Header display:");
    if(sent != PACKET_SIZE)
        fprintf(stderr, HEADER_ERR_MSG, libusb_strerror(sent));
    #endif
    return sent;
}

#ifdef DEBUG
static void print_packet(byte_t *pck, char *str)
{
    byte_t *p;
    puts(str);
    for(p = pck; p < pck+PACKET_SIZE; p++) {
        printf("%02X ", (int)(*p));
        if((p-pck+1) % 16 == 0)
            puts("");
    }
    puts("");
}
#endif

static libusb_device_handle *attempt_reconnect(void)
{
    libusb_device_handle *handle = NULL;
    libusb_device **devs;
    libusb_device *micro_dev;
    int cnt, errcode, retry;

    /* Get device list */
    cnt = libusb_get_device_list(NULL, &devs);
    if(cnt < 0) {
        #ifdef DEBUG
        fprintf(stderr, "Failed to get device list: %s\n", libusb_strerror(cnt));
        #endif
        return NULL;
    }

    /* Search for the device */
    micro_dev = dev_search(devs, cnt);
    if(!micro_dev) {
        libusb_free_device_list(devs, 1);
        return NULL;
    }

    /* Try opening device */
    for(retry = 0; retry < 3; retry++) {
        errcode = libusb_open(micro_dev, &handle);
        if(errcode == 0) {
            /* Device opened, now try to claim interfaces */
            errcode = claim_dev_interface(handle);
            if(errcode == 0) {
                /* Success! */
                libusb_free_device_list(devs, 1);
                return handle;
            }
            libusb_close(handle);
            handle = NULL;
        }
        usleep(200000); /* 200ms delay */
    }

    libusb_free_device_list(devs, 1);
    return NULL;
}
