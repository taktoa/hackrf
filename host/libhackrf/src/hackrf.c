// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// Copyright (c) 2012, Jared Boone <jared@sharebrained.com>
// Copyright (c) 2013, Benjamin Vernoux <titanmkd@gmail.com>
// Copyright (c) 2013, Michael Ossmann <mike@ossmann.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of Great Scott Gadgets nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#include "hackrf.h"

#include <stdlib.h>
#include <string.h>
#include <libusb.h>

#ifdef _WIN32
// Avoid redefinition of timespec from time.h (included by libusb.h)
# define HAVE_STRUCT_TIMESPEC 1
#endif

#include <pthread.h>

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#ifndef bool
typedef int bool;
# define true 1
# define false 0
#endif

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#ifdef HACKRF_BIG_ENDIAN
# define TO_LE32(x) __builtin_bswap32(x)
# define TO_LE64(x) __builtin_bswap64(x)
#else
# define TO_LE32(x) x
# define TO_LE64(x) x
#endif

#define USB_CONFIG_STANDARD 0x1
#define TRANSFER_COUNT 4
#define TRANSFER_BUFFER_SIZE 262144

#define USB_API_REQUIRED(device, version)                           \
    {                                                               \
        uint16_t usb_version = 0;                                   \
        hackrf_usb_api_version_read(device, &usb_version);          \
        if(usb_version < version) {                                 \
            return HACKRF_ERROR_USB_API_VERSION;                    \
        }                                                           \
    }

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// TODO: Factor hackrf_vendor_request into a shared header so that firmware
// can use the same values.

/// @private
typedef enum {
    HACKRF_VENDOR_REQUEST_SET_TRANSCEIVER_MODE          = 1,
    HACKRF_VENDOR_REQUEST_MAX2837_WRITE                 = 2,
    HACKRF_VENDOR_REQUEST_MAX2837_READ                  = 3,
    HACKRF_VENDOR_REQUEST_SI5351C_WRITE                 = 4,
    HACKRF_VENDOR_REQUEST_SI5351C_READ                  = 5,
    HACKRF_VENDOR_REQUEST_SAMPLE_RATE_SET               = 6,
    HACKRF_VENDOR_REQUEST_BASEBAND_FILTER_BANDWIDTH_SET = 7,
    HACKRF_VENDOR_REQUEST_RFFC5071_WRITE                = 8,
    HACKRF_VENDOR_REQUEST_RFFC5071_READ                 = 9,
    HACKRF_VENDOR_REQUEST_SPIFLASH_ERASE                = 10,
    HACKRF_VENDOR_REQUEST_SPIFLASH_WRITE                = 11,
    HACKRF_VENDOR_REQUEST_SPIFLASH_READ                 = 12,
    HACKRF_VENDOR_REQUEST_BOARD_ID_READ                 = 14,
    HACKRF_VENDOR_REQUEST_VERSION_STRING_READ           = 15,
    HACKRF_VENDOR_REQUEST_SET_FREQ                      = 16,
    HACKRF_VENDOR_REQUEST_AMP_ENABLE                    = 17,
    HACKRF_VENDOR_REQUEST_BOARD_PARTID_SERIALNO_READ    = 18,
    HACKRF_VENDOR_REQUEST_SET_LNA_GAIN                  = 19,
    HACKRF_VENDOR_REQUEST_SET_VGA_GAIN                  = 20,
    HACKRF_VENDOR_REQUEST_SET_TXVGA_GAIN                = 21,
    HACKRF_VENDOR_REQUEST_ANTENNA_ENABLE                = 23,
    HACKRF_VENDOR_REQUEST_SET_FREQ_EXPLICIT             = 24,
    // USB_WCID_VENDOR_REQ                              = 25
    HACKRF_VENDOR_REQUEST_INIT_SWEEP                    = 26,
    HACKRF_VENDOR_REQUEST_OPERACAKE_GET_BOARDS          = 27,
    HACKRF_VENDOR_REQUEST_OPERACAKE_SET_PORTS           = 28,
    HACKRF_VENDOR_REQUEST_SET_HW_SYNC_MODE              = 29,
    HACKRF_VENDOR_REQUEST_RESET                         = 30,
    HACKRF_VENDOR_REQUEST_OPERACAKE_SET_RANGES          = 31,
    HACKRF_VENDOR_REQUEST_CLKOUT_ENABLE                 = 32,
    HACKRF_VENDOR_REQUEST_SPIFLASH_STATUS               = 33,
    HACKRF_VENDOR_REQUEST_SPIFLASH_CLEAR_STATUS         = 34,
} hackrf_vendor_request;

/// @private
typedef enum {
    HACKRF_TRANSCEIVER_MODE_OFF = 0,
    HACKRF_TRANSCEIVER_MODE_RECEIVE = 1,
    HACKRF_TRANSCEIVER_MODE_TRANSMIT = 2,
    HACKRF_TRANSCEIVER_MODE_SS = 3,
    TRANSCEIVER_MODE_CPLD_UPDATE = 4,
} hackrf_transceiver_mode;

/// @private
typedef enum {
    HACKRF_HW_SYNC_MODE_OFF = 0,
    HACKRF_HW_SYNC_MODE_ON = 1,
} hackrf_hw_sync_mode;

/// @private
struct hackrf_device {
    libusb_device_handle* usb_device;
    struct libusb_transfer** transfers;
    hackrf_sample_block_cb_fn callback;
    volatile bool transfer_thread_started; // volatile shared between threads (read only)
    pthread_t transfer_thread;
    volatile bool streaming; // volatile shared between threads (read only)
    void* rx_ctx;
    void* tx_ctx;
    unsigned char buffer[TRANSFER_COUNT * TRANSFER_BUFFER_SIZE];
};

/// @private
typedef struct {
    uint32_t bandwidth_hz;
} max2837_ft_t;

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static const max2837_ft_t max2837_ft[] = {
    { 1750000  },
    { 2500000  },
    { 3500000  },
    { 5000000  },
    { 5500000  },
    { 6000000  },
    { 7000000  },
    { 8000000  },
    { 9000000  },
    { 10000000 },
    { 12000000 },
    { 14000000 },
    { 15000000 },
    { 20000000 },
    { 24000000 },
    { 28000000 },
    { 0        }
};

static volatile bool do_exit = false;

static const uint16_t hackrf_usb_vid = 0x1d50;
static const uint16_t hackrf_jawbreaker_usb_pid = 0x604b;
static const uint16_t hackrf_one_usb_pid = 0x6089;
static const uint16_t rad1o_usb_pid = 0xcc15;

static uint16_t open_devices = 0;

static libusb_context* g_libusb_context = NULL;

enum libusb_error last_libusb_error = LIBUSB_SUCCESS;

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static void
request_exit() {
    do_exit = true;
}

static enum hackrf_error
cancel_transfers(hackrf_device* device) {
    // FIXME: what if `device` is NULL?

    if(device->transfers != NULL) {
        {
            uint32_t i;
            for(i = 0; i < TRANSFER_COUNT; i++) {
                if(device->transfers[i] != NULL) {
                    libusb_cancel_transfer(device->transfers[i]);
                }
            }
        }

        return HACKRF_SUCCESS;
    } else {
        return HACKRF_ERROR_OTHER;
    }
}

static enum hackrf_error
free_transfers(hackrf_device* device) {
    // FIXME: what if `device` is NULL?

    if(device->transfers != NULL) {
        // libusb_close() should free all transfers referenced from this array.

        {
            uint32_t i;
            for(i = 0; i < TRANSFER_COUNT; i++) {
                if(device->transfers[i] != NULL) {
                    libusb_free_transfer(device->transfers[i]);
                    device->transfers[i] = NULL;
                }
            }
        }

        free(device->transfers);
        device->transfers = NULL;
    }

    return HACKRF_SUCCESS;
}

static enum hackrf_error
allocate_transfers(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    if(device->transfers == NULL) {
        device->transfers = (struct libusb_transfer**) calloc(TRANSFER_COUNT, sizeof(struct libusb_transfer*));

        if(device->transfers == NULL) {
            return HACKRF_ERROR_NO_MEM;
        }

        {
            uint32_t i;
            for(i = 0; i < TRANSFER_COUNT; i++) {
                device->transfers[i] = libusb_alloc_transfer(0);
                if(device->transfers[i] == NULL) {
                    return HACKRF_ERROR_LIBUSB;
                }

                libusb_fill_bulk_transfer(
                    device->transfers[i],
                    device->usb_device,
                    0,
                    &device->buffer[i * TRANSFER_BUFFER_SIZE],
                    TRANSFER_BUFFER_SIZE,
                    NULL,
                    device,
                    0);

                if(device->transfers[i]->buffer == NULL) {
                    return HACKRF_ERROR_NO_MEM;
                }
            }
        }

        return HACKRF_SUCCESS;
    } else {
        return HACKRF_ERROR_BUSY;
    }
}

static enum hackrf_error
prepare_transfers(hackrf_device*        device,
                  uint_fast8_t          endpoint_address,
                  libusb_transfer_cb_fn callback) {
    // FIXME: what if `device == NULL`?

    if(device->transfers != NULL) {
        {
            uint32_t i;
            for(i = 0; i < TRANSFER_COUNT; i++) {
                device->transfers[i]->endpoint = endpoint_address;
                device->transfers[i]->callback = callback;

                enum libusb_error error
                    = libusb_submit_transfer(device->transfers[i]);
                if(error != 0) {
                    last_libusb_error = error;
                    return HACKRF_ERROR_LIBUSB;
                }
            }
        }
        return HACKRF_SUCCESS;
    } else {
        // This shouldn't happen.
        // FIXME: if that's true, maybe this should be `assert(false)`?
        return HACKRF_ERROR_OTHER;
    }
}

static enum hackrf_error
detach_kernel_drivers(libusb_device_handle* usb_device_handle) {
    // FIXME: what if `usb_device_handle == NULL`?

    int num_interfaces;
    libusb_device* dev;
    struct libusb_config_descriptor* config;

    dev = libusb_get_device(usb_device_handle);

    {
        enum libusb_error result
            = libusb_get_active_config_descriptor(dev, &config);
        if(result < 0) {
            last_libusb_error = result;
            return HACKRF_ERROR_LIBUSB;
        }
    }

    num_interfaces = config->bNumInterfaces;
    libusb_free_config_descriptor(config);

    {
        int i;
        for(i = 0; i < num_interfaces; i++) {
            enum libusb_error result
                = libusb_kernel_driver_active(usb_device_handle, i);
            if(result < 0) {
                if(result == LIBUSB_ERROR_NOT_SUPPORTED) {
                    // FIXME: this if statement could do with some explanation.
                    return HACKRF_SUCCESS;
                }
                last_libusb_error = result;
                return HACKRF_ERROR_LIBUSB;
            } else if(result == 1) {
                result = libusb_detach_kernel_driver(usb_device_handle, i);
                if(result != 0) {
                    last_libusb_error = result;
                    return HACKRF_ERROR_LIBUSB;
                }
            }
        }
    }

    return HACKRF_SUCCESS;
}

static enum hackrf_error
set_hackrf_configuration(libusb_device_handle* usb_device,
                         int                   config) {
    // FIXME: what if `usb_device == NULL`?

    int curr_config = 0;

    {
        enum libusb_error result
            = libusb_get_configuration(usb_device, &curr_config);
        if(result != LIBUSB_SUCCESS) {
            last_libusb_error = result;
            return HACKRF_ERROR_LIBUSB;
        }
    }

    if(curr_config != config) {
        {
            enum hackrf_error result = detach_kernel_drivers(usb_device);
            if(result != HACKRF_SUCCESS) {
                return result;
            }
        }

        {
            enum libusb_error result
                = libusb_set_configuration(usb_device, config);
            if(result != LIBUSB_SUCCESS) {
                last_libusb_error = result;
                return HACKRF_ERROR_LIBUSB;
            }
        }
    }

    {
        enum hackrf_error result = detach_kernel_drivers(usb_device);

        if(result != HACKRF_SUCCESS) {
            return result;
        }
    }

    return HACKRF_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif

enum hackrf_error ADDCALL
hackrf_init() {
    open_devices++;

    if(g_libusb_context != NULL) {
        return HACKRF_SUCCESS;
    }

    enum libusb_error error = libusb_init(&g_libusb_context);
    if(error != 0) {
        last_libusb_error = error;
        open_devices--;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_exit() {
    if(open_devices == 0) {
        if(g_libusb_context != NULL) {
            libusb_exit(g_libusb_context);
            g_libusb_context = NULL;
        }
        return HACKRF_SUCCESS;
    } else {
        return HACKRF_ERROR_NOT_LAST_DEVICE;
    }
}

#ifndef LIBRARY_VERSION
# define LIBRARY_VERSION "unknown"
#endif

const char* ADDCALL
hackrf_library_version() {
    return LIBRARY_VERSION;
}

#ifndef LIBRARY_RELEASE
# define LIBRARY_RELEASE "unknown"
#endif

const char* ADDCALL
hackrf_library_release() {
    return LIBRARY_RELEASE;
}

hackrf_device_list_t* ADDCALL
hackrf_device_list() {
    libusb_device_handle* usb_device = NULL;
    uint8_t serial_descriptor_index;
    char serial_number[64];
    uint8_t idx;

    hackrf_device_list_t* list = calloc(1, sizeof(*list));
    if(list == NULL) {
        return NULL;
    }

    list->usb_devicecount = libusb_get_device_list(g_libusb_context, (libusb_device ***)&list->usb_devices);

    list->serial_numbers = calloc(list->usb_devicecount, sizeof(void *));
    list->usb_board_ids = calloc(list->usb_devicecount, sizeof(enum hackrf_usb_board_id));
    list->usb_device_index = calloc(list->usb_devicecount, sizeof(int));

    if(list->serial_numbers == NULL
       || list->usb_board_ids == NULL
       || list->usb_device_index == NULL) {
        hackrf_device_list_free(list);
        return NULL;
    }

    ssize_t i;
    for(i = 0; i < list->usb_devicecount; i++) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(list->usb_devices[i], &desc);

        const uint16_t vendorID = desc.idVendor;
        const uint16_t productID = desc.idProduct;

        const bool is_hackrf     = (vendorID == hackrf_usb_vid);
        const bool is_hackrf_one = (productID == hackrf_one_usb_pid);
        const bool is_jawbreaker = (productID == hackrf_jawbreaker_usb_pid);
        const bool is_rad1o      = (productID == rad1o_usb_pid);

        if(is_hackrf && (is_hackrf_one || is_jawbreaker || is_rad1o)) {
            idx = list->devicecount++;
            list->usb_board_ids[idx] = productID;
            list->usb_device_index[idx] = i;

            serial_descriptor_index = desc.iSerialNumber;
            if(serial_descriptor_index > 0) {
                if(libusb_open(list->usb_devices[i], &usb_device) != 0) {
                    usb_device = NULL;
                    continue;
                }

                {
                    const uint8_t len = libusb_get_string_descriptor_ascii(
                        usb_device,
                        serial_descriptor_index,
                        (unsigned char*) serial_number,
                        sizeof(serial_number));

                    // FIXME: this if statement could use some explanation
                    if(len == 32) {
                        serial_number[32] = 0;
                        list->serial_numbers[idx] = strdup(serial_number);
                    }
                }

                libusb_close(usb_device);
                usb_device = NULL;
            }
        }
    }

    return list;
}

void ADDCALL
hackrf_device_list_free(hackrf_device_list_t* list) {
    // FIXME: what if `list == NULL`?

    libusb_free_device_list((libusb_device **) list->usb_devices, 1);

    {
        int i;
        for(i = 0; i < list->devicecount; i++) {
            if(list->serial_numbers[i]) {
                free(list->serial_numbers[i]);
            }
        }
    }

    free(list->serial_numbers);
    free(list->usb_board_ids);
    free(list->usb_device_index);
    free(list);
}

static libusb_device_handle*
hackrf_open_usb(const char* desired_serial_number) {
    // FIXME: what if `desired_serial_number == NULL`?

    libusb_device_handle* usb_device = NULL;
    libusb_device** devices = NULL;
    const ssize_t list_length = libusb_get_device_list(g_libusb_context, &devices);
    size_t match_len = 0;
    char serial_number[64];

    if(desired_serial_number) {
        // If a shorter serial number is specified, only match against the suffix.
        // Should probably complain if the match is not unique, currently doesn't.
        match_len = strlen(desired_serial_number);
        if(match_len > 32) {
            return NULL;
        }
    }

    ssize_t i;
    for(i = 0; i < list_length; i++) {
        struct libusb_device_descriptor device_descriptor;
        libusb_get_device_descriptor(devices[i], &device_descriptor);

        if(device_descriptor.idVendor == hackrf_usb_vid) {
            if((device_descriptor.idProduct == hackrf_one_usb_pid)
               || (device_descriptor.idProduct == hackrf_jawbreaker_usb_pid)
               || (device_descriptor.idProduct == rad1o_usb_pid)) {

                if(desired_serial_number != NULL) {
                    const uint_fast8_t serial_descriptor_index
                        = device_descriptor.iSerialNumber;
                    if(serial_descriptor_index > 0) {
                        if(libusb_open(devices[i], &usb_device) != LIBUSB_SUCCESS) {
                            usb_device = NULL;
                            continue;
                        }
                        int serial_number_length
                            = libusb_get_string_descriptor_ascii(
                                usb_device,
                                serial_descriptor_index,
                                (unsigned char*) serial_number,
                                sizeof(serial_number));
                        if(serial_number_length == 32) {
                            serial_number[32] = 0;
                            if(strncmp(serial_number + 32 - match_len, desired_serial_number, match_len) == 0) {
                                break;
                            } else {
                                libusb_close(usb_device);
                                usb_device = NULL;
                            }
                        } else {
                            libusb_close(usb_device);
                            usb_device = NULL;
                        }
                    }
                } else {
                    libusb_open(devices[i], &usb_device);
                    break;
                }
            }
        }
    }

    libusb_free_device_list(devices, 1);

    return usb_device;
}

static enum hackrf_error
hackrf_open_setup(libusb_device_handle* usb_device,
                  hackrf_device**       device) {
    // FIXME: what if `usb_device == NULL`?
    // FIXME: what if `device == NULL`?

    // TODO: Error or warning if not high speed USB?
    // // int speed = libusb_get_device_speed(usb_device);

    {
        enum hackrf_error result
            = set_hackrf_configuration(usb_device, USB_CONFIG_STANDARD);
        if(result != HACKRF_SUCCESS) {
            libusb_close(usb_device);
            return result;
        }
    }

    {
        enum libusb_error result = libusb_claim_interface(usb_device, 0);
        if(result != LIBUSB_SUCCESS) {
            last_libusb_error = result;
            libusb_close(usb_device);
            return HACKRF_ERROR_LIBUSB;
        }
    }

    hackrf_device* lib_device = NULL;
    lib_device = (hackrf_device*) malloc(sizeof(hackrf_device));
    if(lib_device == NULL) {
        libusb_release_interface(usb_device, 0);
        libusb_close(usb_device);
        return HACKRF_ERROR_NO_MEM;
    }

    lib_device->usb_device              = usb_device;
    lib_device->transfers               = NULL;
    lib_device->callback                = NULL;
    lib_device->transfer_thread_started = false;
    lib_device->streaming               = false;

    do_exit = false;

    {
        enum hackrf_error result = allocate_transfers(lib_device);
        if(result != HACKRF_SUCCESS) {
            free(lib_device);
            libusb_release_interface(usb_device, 0);
            libusb_close(usb_device);
            return HACKRF_ERROR_NO_MEM;
        }
    }

    *device = lib_device;

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_open(hackrf_device** device) {
    if(device == NULL) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    // Try opening a HackRF One
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(
        g_libusb_context, hackrf_usb_vid, hackrf_one_usb_pid);

    // If that failed, try opening a HackRF Jawbreaker
    if(handle == NULL) {
        handle = libusb_open_device_with_vid_pid(
            g_libusb_context, hackrf_usb_vid, hackrf_jawbreaker_usb_pid);
    }

    // If that failed, try opening a DEFCON rad1o badge
    if(handle == NULL) {
        handle = libusb_open_device_with_vid_pid(
            g_libusb_context, hackrf_usb_vid, rad1o_usb_pid);
    }

    // If that failed, return `HACKRF_ERROR_NOT_FOUND`.
    if(handle == NULL) {
        return HACKRF_ERROR_NOT_FOUND;
    }

    return hackrf_open_setup(handle, device);
}

enum hackrf_error ADDCALL
hackrf_open_by_serial(const char*     desired_serial_number,
                      hackrf_device** device) {
    libusb_device_handle* usb_device;

    if(desired_serial_number == NULL) {
        return hackrf_open(device);
    }

    if(device == NULL) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    usb_device = hackrf_open_usb(desired_serial_number);

    if(usb_device == NULL) {
        return HACKRF_ERROR_NOT_FOUND;
    }

    return hackrf_open_setup(usb_device, device);
}

enum hackrf_error ADDCALL
hackrf_device_list_open(hackrf_device_list_t* list,
                        int                   idx,
                        hackrf_device**       device) {
    if(device == NULL) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(list == NULL) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(idx < 0) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(idx >= list->devicecount) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    libusb_device_handle* handle = NULL;

    {
        enum libusb_error result = libusb_open(
            list->usb_devices[list->usb_device_index[idx]], &handle);
        if(result != LIBUSB_SUCCESS) {
            handle = NULL; // FIXME: why is this necessary?
            last_libusb_error = result;
            return HACKRF_ERROR_LIBUSB;
        }
    }

    return hackrf_open_setup(handle, device);
}

static enum hackrf_error
hackrf_set_transceiver_mode(hackrf_device*          device,
                            hackrf_transceiver_mode value) {
    // FIXME: what if `device == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_TRANSCEIVER_MODE,
        value,
        0,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_max2837_read(hackrf_device* device,
                    uint8_t        register_number,
                    uint16_t*      value) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `value == NULL`?

    if(register_number >= 32) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_MAX2837_READ,
        0,
        register_number,
        (unsigned char*)value,
        2,
        0);

    if(result < 2) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_max2837_write(hackrf_device* device,
                     uint8_t        register_number,
                     uint16_t       value) {
    // FIXME: what if `device == NULL`?

    if(register_number >= 32) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(value >= 0x400) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_MAX2837_WRITE,
        value,
        register_number,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_si5351c_read(hackrf_device* device,
                    uint16_t       register_number,
                    uint16_t*      value) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `value == NULL`?

    if(register_number >= 256) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    uint8_t temp_value = 0;

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SI5351C_READ,
        0,
        register_number,
        (unsigned char*)&temp_value,
        1,
        0);

    if(result < 1) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    } else {
        *value = temp_value;
        return HACKRF_SUCCESS;
    }
}

enum hackrf_error ADDCALL
hackrf_si5351c_write(hackrf_device* device,
                     uint16_t       register_number,
                     uint16_t       value) {
    // FIXME: what if `device == NULL`?

    if(register_number >= 256) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(value >= 256) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SI5351C_WRITE,
        value,
        register_number,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_baseband_filter_bandwidth(hackrf_device* device,
                                     uint32_t       bandwidth_hz) {
    // FIXME: what if `device == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_BASEBAND_FILTER_BANDWIDTH_SET,
        bandwidth_hz & 0xffff,
        bandwidth_hz >> 16,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}


enum hackrf_error ADDCALL
hackrf_rffc5071_read(hackrf_device* device,
                     uint8_t        register_number,
                     uint16_t*      value) {
    // FIXME: what if `device == NULL`?

    if(register_number >= 31) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_RFFC5071_READ,
        0,
        register_number,
        (unsigned char*)value,
        2,
        0);

    if(result < 2) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_rffc5071_write(hackrf_device* device,
                      uint8_t        register_number,
                      uint16_t       value) {
    // FIXME: what if `device == NULL`?

    if(register_number >= 31) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_RFFC5071_WRITE,
        value,
        register_number,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_spiflash_erase(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SPIFLASH_ERASE,
        0,
        0,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_spiflash_write(hackrf_device* device,
                      uint32_t       address,
                      uint16_t       length,
                      unsigned char* data) {
    // FIXME: what if `device == NULL`?

    if(address > 0x0FFFFF) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SPIFLASH_WRITE,
        address >> 16,
        address & 0xFFFF,
        data,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_spiflash_read(hackrf_device* device,
                     uint32_t       address,
                     uint16_t       length,
                     unsigned char* data) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `data == NULL`?

    if(address > 0x0FFFFF) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SPIFLASH_READ,
        address >> 16,
        address & 0xFFFF,
        data,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_spiflash_status(hackrf_device* device,
                       uint8_t*       data) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `data == NULL`?

    USB_API_REQUIRED(device, 0x0103);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SPIFLASH_STATUS,
        0,
        0,
        data,
        2,
        0);

    // FIXME: why is this <= rather than <?
    if(result <= LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_spiflash_clear_status(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    USB_API_REQUIRED(device, 0x0103);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SPIFLASH_CLEAR_STATUS,
        0,
        0,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_cpld_write(hackrf_device* device,
                  unsigned char* data,
                  unsigned int   total_length) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `data == NULL`?

    const unsigned int chunk_size = 512;
    int transferred = 0;

    {
        enum hackrf_error result
            = hackrf_set_transceiver_mode(device, TRANSCEIVER_MODE_CPLD_UPDATE);

        if(result != HACKRF_SUCCESS) {
            return result;
        }
    }

    unsigned int i;
    for(i = 0; i < total_length; i += chunk_size) {
        enum libusb_error result = libusb_bulk_transfer(
            device->usb_device,
            LIBUSB_ENDPOINT_OUT | 2,
            &data[i],
            chunk_size,
            &transferred,
            10000); // long timeout to allow for CPLD programming

        if(result != LIBUSB_SUCCESS) {
            last_libusb_error = result;
            return HACKRF_ERROR_LIBUSB;
        }
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_board_id_read(hackrf_device* device,
                     uint8_t*       value) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `value == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_BOARD_ID_READ,
        0,
        0,
        value,
        1,
        0);

    if(result <= LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_version_string_read(hackrf_device* device,
                           char*          version,
                           uint8_t        length) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `version == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_VERSION_STRING_READ,
        0,
        0,
        (unsigned char*)version,
        length,
        0);

    if(result < LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    } else {
        version[result] = '\0';
        return HACKRF_SUCCESS;
    }
}

extern ADDAPI enum hackrf_error ADDCALL
hackrf_usb_api_version_read(hackrf_device* device,
                            uint16_t*      version) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `version == NULL`?

    libusb_device* dev;
    struct libusb_device_descriptor desc;
    dev = libusb_get_device(device->usb_device);

    enum libusb_error result = libusb_get_device_descriptor(dev, &desc);
    if(result < LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    *version = desc.bcdDevice;
    return HACKRF_SUCCESS;
}

/// @private
typedef struct {
    uint32_t freq_mhz; // From 0 to 6000+MHz
    uint32_t freq_hz; // From 0 to 999999Hz
    // Final Freq = freq_mhz+freq_hz
} set_freq_params_t;

#define FREQ_ONE_MHZ (1000*1000ull)

enum hackrf_error ADDCALL
hackrf_set_freq(hackrf_device* device,
                uint64_t       freq_hz) {
    // FIXME: what if `device == NULL`?

    // Convert Freq Hz 64bits to Freq MHz (32bits) & Freq Hz (32bits)
    uint32_t l_freq_mhz = (uint32_t)(freq_hz / FREQ_ONE_MHZ);
    uint32_t l_freq_hz = (uint32_t)(freq_hz - (((uint64_t)l_freq_mhz) * FREQ_ONE_MHZ));

    set_freq_params_t set_freq_params;
    set_freq_params.freq_mhz = TO_LE32(l_freq_mhz);
    set_freq_params.freq_hz = TO_LE32(l_freq_hz);

    uint8_t length = sizeof(set_freq_params_t);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_FREQ,
        0,
        0,
        (unsigned char*)&set_freq_params,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

/// @private
struct set_freq_explicit_params {
    uint64_t if_freq_hz; // intermediate frequency
    uint64_t lo_freq_hz; // front-end local oscillator frequency
    uint8_t path;        // image rejection filter path
};

enum hackrf_error ADDCALL
hackrf_set_freq_explicit(hackrf_device*      device,
                         uint64_t            if_freq_hz,
                         uint64_t            lo_freq_hz,
                         enum rf_path_filter path) {
    // FIXME: what if `device == NULL`?

    if(if_freq_hz < 2150000000 || if_freq_hz > 2750000000) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if((path != RF_PATH_FILTER_BYPASS)
       && (lo_freq_hz < 84375000 || lo_freq_hz > 5400000000)) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(path > RF_PATH_FILTER_HIGH_PASS) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    struct set_freq_explicit_params params;
    params.if_freq_hz = TO_LE32(if_freq_hz);
    params.lo_freq_hz = TO_LE32(lo_freq_hz);
    params.path = (uint8_t) path;

    uint8_t length = sizeof(struct set_freq_explicit_params);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_FREQ_EXPLICIT,
        0,
        0,
        (unsigned char*)&params,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

/// @private
typedef struct {
    uint32_t freq_hz;
    uint32_t divider;
} set_fracrate_params_t;

// You should probably use hackrf_set_sample_rate() below instead of this
// function. They both result in automatic baseband filter selection as
// described below.
enum hackrf_error ADDCALL
hackrf_set_sample_rate_manual(hackrf_device* device,
                              uint32_t       freq_hz,
                              uint32_t       divider) {
    // FIXME: what if `device == NULL`?

    set_fracrate_params_t set_fracrate_params;
    set_fracrate_params.freq_hz = TO_LE32(freq_hz);
    set_fracrate_params.divider = TO_LE32(divider);

    uint8_t length = sizeof(set_fracrate_params_t);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SAMPLE_RATE_SET,
        0,
        0,
        (unsigned char*)&set_fracrate_params,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    } else {
        return hackrf_set_baseband_filter_bandwidth(device, hackrf_compute_baseband_filter_bw((uint32_t)(0.75*freq_hz/divider)));
    }
}

// For anti-aliasing, the baseband filter bandwidth is automatically set to the
// widest available setting that is no more than 75% of the sample rate.  This
// happens every time the sample rate is set. If you want to override the
// baseband filter selection, you must do so after setting the sample rate.
enum hackrf_error ADDCALL
hackrf_set_sample_rate(hackrf_device* device,
                       double         freq) {
    // FIXME: what if `device == NULL`?

    const int MAX_N = 32;
    uint32_t freq_hz, divider;
    double freq_frac = 1.0 + freq - (int)freq;
    uint64_t a, m;

    union {
        uint64_t u64;
        double d;
    } v;
    v.d = freq;

    int e = (v.u64 >> 52) - 1023;

    m = ((1ULL << 52) - 1);

    v.d = freq_frac;
    v.u64 &= m;

    m &= ~((1 << (e + 4)) - 1);

    a = 0;

    int i;
    for(i = 1; i < MAX_N; i++) {
        a += v.u64;
        if(!(a & m) || !(~a & m)) {
            break;
        }
    }

    if(i == MAX_N) {
        i = 1;
    }

    freq_hz = (uint32_t)(freq * i + 0.5);
    divider = i;

    return hackrf_set_sample_rate_manual(device, freq_hz, divider);
}

enum hackrf_error ADDCALL
hackrf_set_amp_enable(hackrf_device* device,
                      uint8_t        value) {
    // FIXME: what if `device == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_AMP_ENABLE,
        value,
        0,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_board_partid_serialno_read(hackrf_device*          device,
                                  read_partid_serialno_t* read_partid_serialno) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `read_partid_serialno == NULL`?

    uint8_t length = sizeof(read_partid_serialno_t);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_BOARD_PARTID_SERIALNO_READ,
        0,
        0,
        (unsigned char*)read_partid_serialno,
        length,
        0);

    if(result < length) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    } else {
        read_partid_serialno->part_id[0] = TO_LE32(read_partid_serialno->part_id[0]);
        read_partid_serialno->part_id[1] = TO_LE32(read_partid_serialno->part_id[1]);
        read_partid_serialno->serial_no[0] = TO_LE32(read_partid_serialno->serial_no[0]);
        read_partid_serialno->serial_no[1] = TO_LE32(read_partid_serialno->serial_no[1]);
        read_partid_serialno->serial_no[2] = TO_LE32(read_partid_serialno->serial_no[2]);
        read_partid_serialno->serial_no[3] = TO_LE32(read_partid_serialno->serial_no[3]);

        return HACKRF_SUCCESS;
    }
}

enum hackrf_error ADDCALL
hackrf_set_lna_gain(hackrf_device* device,
                    uint32_t       value) {
    // FIXME: what if `device == NULL`?

    if(value > 40) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    value &= ~0x07;

    uint8_t retval = 0;

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_LNA_GAIN,
        0,
        value,
        &retval,
        1,
        0);

    if((result != 1) || (retval == 0)) {
        // FIXME: shouldn't this be HACKRF_ERROR_LIBUSB?
        return HACKRF_ERROR_INVALID_PARAM;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_vga_gain(hackrf_device* device,
                    uint32_t       value) {
    // FIXME: what if `device == NULL`?

    if(value > 62) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    value &= ~0x01;

    uint8_t retval = 0;

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_VGA_GAIN,
        0,
        value,
        &retval,
        1,
        0);

    if((result != 1) || (retval == 0)) {
        // FIXME: shouldn't this be HACKRF_ERROR_LIBUSB?
        return HACKRF_ERROR_INVALID_PARAM;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_txvga_gain(hackrf_device* device,
                      uint32_t       value) {
    // FIXME: what if `device == NULL`?

    if(value > 47) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    uint8_t retval = 0;

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_TXVGA_GAIN,
        0,
        value,
        &retval,
        1,
        0);

    if((result != 1) || (retval == 0)) {
        // FIXME: shouldn't this be HACKRF_ERROR_LIBUSB?
        return HACKRF_ERROR_INVALID_PARAM;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_antenna_enable(hackrf_device* device,
                          uint8_t        value) {
    // FIXME: what if `device == NULL`?

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_ANTENNA_ENABLE,
        value,
        0,
        NULL,
        0,
        0);

    if(result != 0) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

static void*
transfer_threadproc(void* arg) {
    // FIXME: what if `arg == NULL`?

    hackrf_device* device = (hackrf_device*) arg;
    struct timeval timeout = { 0, 500000 };

    while(device->streaming && (do_exit == false)) {
        enum libusb_error result
            = libusb_handle_events_timeout(g_libusb_context, &timeout);
        if((result != LIBUSB_SUCCESS) && (result != LIBUSB_ERROR_INTERRUPTED)) {
            device->streaming = false;
        }
    }

    return NULL;
}

static void LIBUSB_CALL
hackrf_libusb_transfer_callback(struct libusb_transfer* usb_transfer) {
    // FIXME: what if `usb_transfer == NULL`?

    hackrf_device* device = (hackrf_device*) usb_transfer->user_data;

    if(usb_transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        hackrf_transfer transfer = {
            .device = device,
            .buffer = usb_transfer->buffer,
            .buffer_length = usb_transfer->length,
            .valid_length = usb_transfer->actual_length,
            .rx_ctx = device->rx_ctx,
            .tx_ctx = device->tx_ctx
        };

        if(device->callback(&transfer) == 0) {
            if(libusb_submit_transfer(usb_transfer) < 0) {
                request_exit();
            } else {
                return;
            }
        } else {
            request_exit();
        }
    } else {
        // Other cases:
        //   - LIBUSB_TRANSFER_NO_DEVICE
        //   - LIBUSB_TRANSFER_ERROR
        //   - LIBUSB_TRANSFER_TIMED_OUT
        //   - LIBUSB_TRANSFER_STALL
        //   - LIBUSB_TRANSFER_OVERFLOW
        //   - LIBUSB_TRANSFER_CANCELLED
        //   - ...
        request_exit(); // Fatal error, stop transfer
    }
}

static enum hackrf_error
kill_transfer_thread(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    request_exit();

    if(device->transfer_thread_started != false) {
        void* value = NULL;

        if(pthread_join(device->transfer_thread, &value) != 0) {
            return HACKRF_ERROR_THREAD;
        }

        device->transfer_thread_started = false;

        cancel_transfers(device);
    }

    return HACKRF_SUCCESS;
}

static enum hackrf_error
create_transfer_thread(hackrf_device*            device,
                       uint8_t                   endpoint_address,
                       hackrf_sample_block_cb_fn callback) {
    // FIXME: what if `device == NULL`?

    if(device->transfer_thread_started == false) {
        device->streaming = false;
        do_exit = false;

        {
            enum hackrf_error result = prepare_transfers(device, endpoint_address, hackrf_libusb_transfer_callback);

            if(result != HACKRF_SUCCESS) {
                return result;
            }
        }

        device->streaming = true;
        device->callback = callback;
        if(pthread_create(&device->transfer_thread, 0, transfer_threadproc, device) == 0) {
            device->transfer_thread_started = true;
        } else {
            return HACKRF_ERROR_THREAD;
        }
    } else {
        return HACKRF_ERROR_BUSY;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_is_streaming(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    // return hackrf is streaming only when streaming, transfer_thread_started are true and do_exit equal false

    if((device->transfer_thread_started == true)
       && (device->streaming == true)
       && (do_exit == false)) {
        return HACKRF_TRUE;
    } else {
        if(device->transfer_thread_started == false) {
            return HACKRF_ERROR_STREAMING_THREAD_ERR;
        }

        if(device->streaming == false) {
            return HACKRF_ERROR_STREAMING_STOPPED;
        }

        return HACKRF_ERROR_STREAMING_EXIT_CALLED;
    }
}

enum hackrf_error ADDCALL
hackrf_start_rx(hackrf_device*            device,
                hackrf_sample_block_cb_fn callback,
                void*                     rx_ctx) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `rx_ctx == NULL`?

    const uint8_t endpoint_address = LIBUSB_ENDPOINT_IN | 1;

    enum hackrf_error result
        = hackrf_set_transceiver_mode(device, HACKRF_TRANSCEIVER_MODE_RECEIVE);

    if(result != HACKRF_SUCCESS) {
        return result;
    }

    device->rx_ctx = rx_ctx;
    return create_transfer_thread(device, endpoint_address, callback);
}

enum hackrf_error ADDCALL
hackrf_stop_rx(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    enum hackrf_error result
        = hackrf_set_transceiver_mode(device, HACKRF_TRANSCEIVER_MODE_OFF);

    if(result != HACKRF_SUCCESS) {
        return result;
    }

    return kill_transfer_thread(device);
}

enum hackrf_error ADDCALL
hackrf_start_tx(hackrf_device*            device,
                hackrf_sample_block_cb_fn callback,
                void*                     tx_ctx) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `tx_ctx == NULL`?

    const uint8_t endpoint_address = LIBUSB_ENDPOINT_OUT | 2;

    enum hackrf_error result
        = hackrf_set_transceiver_mode(device, HACKRF_TRANSCEIVER_MODE_TRANSMIT);

    if(result != HACKRF_SUCCESS) {
        return result;
    }

    device->tx_ctx = tx_ctx;
    return create_transfer_thread(device, endpoint_address, callback);
}

enum hackrf_error ADDCALL
hackrf_stop_tx(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    enum hackrf_error result1 = kill_transfer_thread(device);
    enum hackrf_error result2
        = hackrf_set_transceiver_mode(device, HACKRF_TRANSCEIVER_MODE_OFF);

    if(result2 != HACKRF_SUCCESS) {
        return result2;
    }

    return result1;
}

enum hackrf_error ADDCALL
hackrf_close(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    enum hackrf_error result1 = HACKRF_SUCCESS;
    enum hackrf_error result2 = HACKRF_SUCCESS;

    if(device != NULL) {
        result1 = hackrf_stop_rx(device);
        result2 = hackrf_stop_tx(device);

        if(device->usb_device != NULL) {
            libusb_release_interface(device->usb_device, 0);
            libusb_close(device->usb_device);
            device->usb_device = NULL;
        }

        free_transfers(device);

        free(device);
    }

    open_devices--;

    if(result2 != HACKRF_SUCCESS) {
        return result2;
    }

    return result1;
}

const char* ADDCALL
hackrf_error_name(enum hackrf_error errcode) {
    switch(errcode) {
    case HACKRF_SUCCESS:
        return "HACKRF_SUCCESS";

    case HACKRF_TRUE:
        return "HACKRF_TRUE";

    case HACKRF_ERROR_INVALID_PARAM:
        return "invalid parameter(s)";

    case HACKRF_ERROR_NOT_FOUND:
        return "HackRF not found";

    case HACKRF_ERROR_BUSY:
        return "HackRF busy";

    case HACKRF_ERROR_NO_MEM:
        return "insufficient memory";

    case HACKRF_ERROR_LIBUSB:
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000103)
        if(last_libusb_error != LIBUSB_SUCCESS) {
            return libusb_strerror(last_libusb_error);
        }
#endif
        return "USB error";

    case HACKRF_ERROR_THREAD:
        return "transfer thread error";

    case HACKRF_ERROR_STREAMING_THREAD_ERR:
        return "streaming thread encountered an error";

    case HACKRF_ERROR_STREAMING_STOPPED:
        return "streaming stopped";

    case HACKRF_ERROR_STREAMING_EXIT_CALLED:
        return "streaming terminated";

    case HACKRF_ERROR_USB_API_VERSION:
        return "feature not supported by installed firmware";

    case HACKRF_ERROR_NOT_LAST_DEVICE:
        return "some hackrf device is still in use";

    case HACKRF_ERROR_OTHER:
        return "unspecified error";

    default:
        return "unknown error code";
    }
}

const char* ADDCALL
hackrf_board_id_name(enum hackrf_board_id board_id) {
    switch(board_id) {
    case BOARD_ID_JELLYBEAN:
        return "Jellybean";

    case BOARD_ID_JAWBREAKER:
        return "Jawbreaker";

    case BOARD_ID_HACKRF_ONE:
        return "HackRF One";

    case BOARD_ID_RAD1O:
        return "rad1o";

    case BOARD_ID_INVALID:
        return "Invalid Board ID";

    default:
        return "Unknown Board ID";
    }
}

extern ADDAPI const char* ADDCALL
hackrf_usb_board_id_name(enum hackrf_usb_board_id usb_board_id) {
    switch(usb_board_id) {
    case USB_BOARD_ID_JAWBREAKER:
        return "Jawbreaker";

    case USB_BOARD_ID_HACKRF_ONE:
        return "HackRF One";

    case USB_BOARD_ID_RAD1O:
        return "rad1o";

    case USB_BOARD_ID_INVALID:
        return "Invalid Board ID";

    default:
        return "Unknown Board ID";
    }
}

const char* ADDCALL
hackrf_filter_path_name(enum rf_path_filter path) {
    switch(path) {
    case RF_PATH_FILTER_BYPASS:
        return "mixer bypass";
    case RF_PATH_FILTER_LOW_PASS:
        return "low pass filter";
    case RF_PATH_FILTER_HIGH_PASS:
        return "high pass filter";
    default:
        return "invalid filter path";
    }
}

// Return final bw round down and less than expected bw.
uint32_t ADDCALL
hackrf_compute_baseband_filter_bw_round_down_lt(uint32_t bandwidth_hz) {
    const max2837_ft_t* p = max2837_ft;
    while(p->bandwidth_hz != 0) {
        if(p->bandwidth_hz >= bandwidth_hz) {
            break;
        }
        p++;
    }
    // Round down (if no equal to first entry)
    if(p != max2837_ft) {
        p--;
    }
    return p->bandwidth_hz;
}

// Return final bw.
uint32_t ADDCALL
hackrf_compute_baseband_filter_bw(uint32_t bandwidth_hz) {
    const max2837_ft_t* p = max2837_ft;

    while(p->bandwidth_hz != 0) {
        if(p->bandwidth_hz >= bandwidth_hz) {
            break;
        }
        p++;
    }

    // Round down (if no equal to first entry) and if > bandwidth_hz
    if(p != max2837_ft) {
        if(p->bandwidth_hz > bandwidth_hz) {
            p--;
        }
    }

    return p->bandwidth_hz;
}

enum hackrf_error ADDCALL
hackrf_set_hw_sync_mode(hackrf_device* device,
                        uint8_t        value) {
    // FIXME: what if `device == NULL`?

    USB_API_REQUIRED(device, 0x0102);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_SET_HW_SYNC_MODE,
        value,
        0,
        NULL,
        0,
        0);

    if(result != LIBUSB_SUCCESS) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

// Initialize sweep mode:
// frequency_list is a list of start/stop pairs of frequencies in MHz.
// num_ranges is the number of pairs in frequency_list (1 to 10)
// num_bytes is the number of sample bytes to capture after each tuning.
// step_width is the width in Hz of the tuning step.
// offset is a number of Hz added to every tuning frequency.
//   Use to select center frequency based on the expected usable bandwidth.
// sweep_mode
//   LINEAR means step_width is added to the current frequency at each step.
//   INTERLEAVED invokes a scheme in which each step is divided into two
//   interleaved sub-steps, allowing the host to select the best portions
//   of the FFT of each sub-step and discard the rest.
enum hackrf_error ADDCALL
hackrf_init_sweep(hackrf_device*   device,
                  const uint16_t*  frequency_list,
                  int              num_ranges,
                  uint32_t         num_bytes,
                  uint32_t         step_width,
                  uint32_t         offset,
                  enum sweep_style style) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `frequency_list == NULL`?

    USB_API_REQUIRED(device, 0x0102);

    unsigned char data[9 + MAX_SWEEP_RANGES * 2 * sizeof(frequency_list[0])];
    int size = 9 + num_ranges * 2 * sizeof(frequency_list[0]);

    if((num_ranges < 1) || (num_ranges > MAX_SWEEP_RANGES)){
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(num_bytes % BYTES_PER_BLOCK) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(BYTES_PER_BLOCK > num_bytes) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(1 > step_width) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    if(INTERLEAVED < style) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    data[0] = step_width & 0xff;
    data[1] = (step_width >> 8) & 0xff;
    data[2] = (step_width >> 16) & 0xff;
    data[3] = (step_width >> 24) & 0xff;
    data[4] = offset & 0xff;
    data[5] = (offset >> 8) & 0xff;
    data[6] = (offset >> 16) & 0xff;
    data[7] = (offset >> 24) & 0xff;
    data[8] = style;

    {
        int i;
        for(i = 0; i < (2 * num_ranges); i++) {
            data[9  + i * 2] = frequency_list[i] & 0xff;
            data[10 + i * 2] = (frequency_list[i] >> 8) & 0xff;
        }
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_INIT_SWEEP,
        num_bytes & 0xffff,
        (num_bytes >> 16) & 0xffff,
        data,
        size,
        0);

    if(result < size) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

// Retrieve list of Operacake board addresses
// boards must be *uint8_t[8]
enum hackrf_error ADDCALL
hackrf_get_operacake_boards(hackrf_device* device,
                            uint8_t*       boards) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `boards == NULL`?

    USB_API_REQUIRED(device, 0x0102);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_OPERACAKE_GET_BOARDS,
        0,
        0,
        boards,
        8,
        0);

    if(result < 8) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

// Set Operacake ports
enum hackrf_error ADDCALL
hackrf_set_operacake_ports(hackrf_device*       device,
                           uint8_t              address,
                           enum operacake_ports port_a,
                           enum operacake_ports port_b) {
    // FIXME: what if `device == NULL`?

    USB_API_REQUIRED(device, 0x0102);

    // port_a and port_b must both be valid
    if((port_a > OPERACAKE_PB4) || (port_b > OPERACAKE_PB4)) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    // port_a and port_b cannot both be on the PAn side
    if((port_a <= OPERACAKE_PA4) && (port_b <= OPERACAKE_PA4)) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    // port_a and port_b cannot both be on the PBn side
    if((port_a >= OPERACAKE_PB1) && (port_b >= OPERACAKE_PB1)) {
        return HACKRF_ERROR_INVALID_PARAM;
    }

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_OPERACAKE_SET_PORTS,
        address,
        port_a | (port_b << 8),
        NULL,
        0,
        0);

    if(result != 0) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_reset(hackrf_device* device) {
    // FIXME: what if `device == NULL`?

    USB_API_REQUIRED(device, 0x0102);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_RESET,
        0,
        0,
        NULL,
        0,
        0);

    if(result != 0) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_operacake_ranges(hackrf_device* device,
                            uint8_t*       ranges,
                            uint8_t        len_ranges) {
    // FIXME: what if `device == NULL`?
    // FIXME: what if `ranges == NULL`?

    USB_API_REQUIRED(device, 0x0103);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_OPERACAKE_SET_RANGES,
        0,
        0,
        ranges,
        len_ranges,
        0);

    if(result < len_ranges) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

enum hackrf_error ADDCALL
hackrf_set_clkout_enable(hackrf_device* device,
                         uint8_t        value) {
    // FIXME: what if `device == NULL`?

    USB_API_REQUIRED(device, 0x0103);

    enum libusb_error result = libusb_control_transfer(
        device->usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        HACKRF_VENDOR_REQUEST_CLKOUT_ENABLE,
        value,
        0,
        NULL,
        0,
        0);

    if(result != 0) {
        last_libusb_error = result;
        return HACKRF_ERROR_LIBUSB;
    }

    return HACKRF_SUCCESS;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#ifdef __cplusplus
} // __cplusplus defined.
#endif

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
