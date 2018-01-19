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

#ifndef HACKRF_H
#define HACKRF_H

#include <stdint.h>

#ifdef _WIN32
# define ADD_EXPORTS

// You should define ADD_EXPORTS *only* when building the DLL.
# ifdef ADD_EXPORTS
#  define ADDAPI __declspec(dllexport)
# else
#  define ADDAPI __declspec(dllimport)
# endif

// Define calling convention in one place, for convenience.
# define ADDCALL __cdecl

#else // _WIN32 not defined.

// Define with no value on non-Windows OSes.
# define ADDAPI
# define ADDCALL

#endif

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/// FIXME: doc
#define SAMPLES_PER_BLOCK 8192

/// FIXME: doc
#define BYTES_PER_BLOCK 16384

/// FIXME: doc
#define MAX_SWEEP_RANGES 10

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/// FIXME: doc
enum hackrf_error {
    HACKRF_SUCCESS                     = 0,
    HACKRF_TRUE                        = 1,
    HACKRF_ERROR_INVALID_PARAM         = -2,
    HACKRF_ERROR_NOT_FOUND             = -5,
    HACKRF_ERROR_BUSY                  = -6,
    HACKRF_ERROR_NO_MEM                = -11,
    HACKRF_ERROR_LIBUSB                = -1000,
    HACKRF_ERROR_THREAD                = -1001,
    HACKRF_ERROR_STREAMING_THREAD_ERR  = -1002,
    HACKRF_ERROR_STREAMING_STOPPED     = -1003,
    HACKRF_ERROR_STREAMING_EXIT_CALLED = -1004,
    HACKRF_ERROR_USB_API_VERSION       = -1005,
    HACKRF_ERROR_NOT_LAST_DEVICE       = -2000,
    HACKRF_ERROR_OTHER                 = -9999,
};

/// FIXME: doc
enum hackrf_board_id {
    BOARD_ID_JELLYBEAN  = 0,
    BOARD_ID_JAWBREAKER = 1,
    BOARD_ID_HACKRF_ONE = 2,
    BOARD_ID_RAD1O      = 3,
    BOARD_ID_INVALID    = 0xFF,
};

/// FIXME: doc
enum hackrf_usb_board_id {
    USB_BOARD_ID_JAWBREAKER = 0x604B,
    USB_BOARD_ID_HACKRF_ONE = 0x6089,
    USB_BOARD_ID_RAD1O      = 0xCC15,
    USB_BOARD_ID_INVALID    = 0xFFFF,
};

/// FIXME: doc
enum rf_path_filter {
    /// FIXME: doc
    RF_PATH_FILTER_BYPASS = 0,

    /// FIXME: doc
    RF_PATH_FILTER_LOW_PASS = 1,

    /// FIXME: doc
    RF_PATH_FILTER_HIGH_PASS = 2,
};

/// FIXME: doc
enum operacake_ports {
    OPERACAKE_PA1 = 0,
    OPERACAKE_PA2 = 1,
    OPERACAKE_PA3 = 2,
    OPERACAKE_PA4 = 3,
    OPERACAKE_PB1 = 4,
    OPERACAKE_PB2 = 5,
    OPERACAKE_PB3 = 6,
    OPERACAKE_PB4 = 7,
};

/// FIXME: doc
enum sweep_style {
    /// FIXME: doc
    LINEAR      = 0,

    /// FIXME: doc
    INTERLEAVED = 1,
};

/// FIXME: doc
typedef struct hackrf_device hackrf_device;

/// FIXME: doc
typedef struct {
    /// FIXME: doc
    hackrf_device* device;

    /// FIXME: doc
    uint8_t* buffer;

    /// FIXME: doc
    int buffer_length;

    /// FIXME: doc
    int valid_length;

    /// FIXME: doc
    void* rx_ctx;

    /// FIXME: doc
    void* tx_ctx;
} hackrf_transfer;

// FIXME: remove this
typedef struct {
    uint32_t part_id[2];
    uint32_t serial_no[4];
} read_partid_serialno_t;

/// FIXME: doc
struct hackrf_device_list {
    /// FIXME: doc
    char** serial_numbers;

    /// FIXME: doc
    enum hackrf_usb_board_id* usb_board_ids;

    /// FIXME: doc
    int* usb_device_index;

    /// FIXME: doc
    int devicecount;

    /// FIXME: doc
    void** usb_devices;

    /// FIXME: doc
    int usb_devicecount;
};

/// FIXME: doc
typedef struct hackrf_device_list hackrf_device_list_t;

/// FIXME: doc
///
/// If the return value of a callback of this type is anything other than 0,
/// the transfer will be stopped (and that returned value is thrown away).
typedef int (*hackrf_sample_block_cb_fn)(hackrf_transfer* transfer);

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C"
{
#endif

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_init(void);

/// \brief FIXME: doc
///
/// \returns \link HACKRF_SUCCESS \endlink unconditionally.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_exit(void);

/// \brief FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_library_version(void);

/// \brief FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_library_release(void);

// -----------------------------------------------------------------------------
// ---- Functions related to enumerating HackRF devices ------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI hackrf_device_list_t* ADDCALL
hackrf_device_list(void);

/// \brief FIXME: doc
///
/// \param list   FIXME: doc
/// \param idx    FIXME: doc
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `device == NULL`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `list == NULL`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `idx < 0`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `idx >= list->devicecount`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_ERROR_NO_MEM \endlink
///          if `malloc` failed to allocate enough memory.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_device_list_open(hackrf_device_list_t* list,
                        int                   idx,
                        hackrf_device**       device);

/// \brief FIXME: doc
///
/// \param list FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI void ADDCALL
hackrf_device_list_free(hackrf_device_list_t* list);

// -----------------------------------------------------------------------------
// ---- Functions related to connecting to a HackRF device ---------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `device == NULL`.
/// \returns \link HACKRF_ERROR_NOT_FOUND \endlink
///          if neither a HackRF One, nor a HackRF Jawbreaker,
///          nor a DEFCON rad1o were found by the USB controller.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_ERROR_NO_MEM \endlink
///          if `malloc` failed to allocate enough memory.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_open(hackrf_device** device);

/// \brief FIXME: doc
///
/// \param desired_serial_number FIXME: doc
/// \param device                FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `device == NULL`.
/// \returns \link HACKRF_ERROR_NOT_FOUND \endlink
///          if neither a HackRF One, nor a HackRF Jawbreaker,
///          nor a DEFCON rad1o were found by the USB controller.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_ERROR_NO_MEM \endlink
///          if `malloc` failed to allocate enough memory.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_open_by_serial(const char*     desired_serial_number,
                      hackrf_device** device);

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_THREAD \endlink
///          if `pthread_join`ing the transfer thread failed.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_close(hackrf_device* device);

// -----------------------------------------------------------------------------
// ---- Functions related to streaming -----------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device   FIXME: doc
/// \param callback FIXME: doc
/// \param rx_ctx   FIXME: doc
///
/// \returns \link HACKRF_ERROR_THREAD \endlink
///          if `pthread_create`ing the transfer thread failed.
/// \returns \link HACKRF_ERROR_BUSY \endlink
///          if the transfer thread was already started.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_ERROR_OTHER \endlink
///          if something that should never happen happens.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_start_rx(hackrf_device*            device,
                hackrf_sample_block_cb_fn callback,
                void*                     rx_ctx);

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_THREAD \endlink
///          if `pthread_join`ing the transfer thread failed.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_stop_rx(hackrf_device* device);

/// \brief FIXME: doc
///
/// \param device   FIXME: doc
/// \param callback FIXME: doc
/// \param tx_ctx   FIXME: doc
///
/// \returns \link HACKRF_ERROR_THREAD \endlink
///          if `pthread_create`ing the transfer thread failed.
/// \returns \link HACKRF_ERROR_BUSY \endlink
///          if the transfer thread was already started.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_ERROR_OTHER \endlink
///          if something that should never happen happens.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_start_tx(hackrf_device*            device,
                hackrf_sample_block_cb_fn callback,
                void*                     tx_ctx);

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_THREAD \endlink
///          if `pthread_join`ing the transfer thread failed.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_stop_tx(hackrf_device* device);

/// \brief FIXME: doc
///
/// FIXME: maybe this should return a different enum
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_TRUE \endlink
///          if the transfer thread is running and the device is streaming
///          and the transfer thread has not requested that it be killed.
/// \returns \link HACKRF_ERROR_STREAMING_THREAD_ERR \endlink
///          if the transfer thread is not running.
/// \returns \link HACKRF_ERROR_STREAMING_STOPPED \endlink
///          if the device is not streaming.
/// \returns \link HACKRF_ERROR_STREAMING_EXIT_CALLED \endlink
///          if the transfer thread has requested that it be killed.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_is_streaming(hackrf_device* device);

// -----------------------------------------------------------------------------
// ---- MAX2837-related functions ----------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 32`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_max2837_read(hackrf_device* device,
                    uint8_t        register_number,
                    uint16_t*      value);

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 32`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `value >= 1024`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_max2837_write(hackrf_device* device,
                     uint8_t        register_number,
                     uint16_t       value);

// -----------------------------------------------------------------------------
// ---- Si5351C-related functions ----------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 256`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_si5351c_read(hackrf_device* device,
                    uint16_t       register_number,
                    uint16_t*      value);

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 256`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `value >= 256`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_si5351c_write(hackrf_device* device,
                     uint16_t       register_number,
                     uint16_t       value);

// -----------------------------------------------------------------------------
// ---- Baseband filter-related functions --------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device       FIXME: doc
/// \param bandwidth_hz FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_baseband_filter_bandwidth(hackrf_device* device,
                                     uint32_t       bandwidth_hz);

// -----------------------------------------------------------------------------
// ---- RFFC5071-related functions ---------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 31`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_rffc5071_read(hackrf_device* device,
                     uint8_t        register_number,
                     uint16_t*      value);

/// \brief FIXME: doc
///
/// \param device          FIXME: doc
/// \param register_number FIXME: doc
/// \param value           FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 31`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_rffc5071_write(hackrf_device* device,
                      uint8_t        register_number,
                      uint16_t       value);

// -----------------------------------------------------------------------------
// ---- SPI flash memory-related functions -------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_spiflash_erase(hackrf_device* device);

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param address FIXME: doc
/// \param length  FIXME: doc
/// \param data    FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 1048575`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_spiflash_read(hackrf_device* device,
                     uint32_t       address,
                     uint16_t       length,
                     unsigned char* data);

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param address FIXME: doc
/// \param length  FIXME: doc
/// \param data    FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `register_number >= 1048575`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_spiflash_write(hackrf_device* device,
                      uint32_t       address,
                      uint16_t       length,
                      unsigned char* data);

/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1003 or higher
///
/// \param device FIXME: doc
/// \param data   FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1003`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_spiflash_status(hackrf_device* device,
                       uint8_t*       data);

/// \brief FIXME: doc
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1003`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_spiflash_clear_status(hackrf_device* device);

// -----------------------------------------------------------------------------
// ---- CPLD-related functions -------------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// device will need to be reset after hackrf_cpld_write
///
/// \param device       FIXME: doc
/// \param data         FIXME: doc
/// \param total_length FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_cpld_write(hackrf_device* device,
                  unsigned char* data,
                  unsigned int   total_length);

// -----------------------------------------------------------------------------
// ---- Board metadata-related functions ---------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_board_id_read(hackrf_device* device,
                     uint8_t* value);

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param version FIXME: doc
/// \param length  FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_version_string_read(hackrf_device* device,
                           char*          version,
                           uint8_t        length);

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param version FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_usb_api_version_read(hackrf_device* device,
                            uint16_t*      version);

/// \brief FIXME: doc
///
/// \param device               FIXME: doc
/// \param read_partid_serialno FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_board_partid_serialno_read(hackrf_device*          device,
                                  read_partid_serialno_t* read_partid_serialno);

// -----------------------------------------------------------------------------
// ---- RF-related functions ---------------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param freq_hz FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_freq(hackrf_device* device,
                uint64_t       freq_hz);

/// \brief FIXME: doc
///
/// \param device     FIXME: doc
/// \param if_freq_hz FIXME: doc
/// \param lo_freq_hz FIXME: doc
/// \param path       FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `if_freq_hz < 2150000000`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `if_freq_hz > 2750000000`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `path` is not \link RF_PATH_FILTER_BYPASS \endlink
///          and `lo_freq_hz < 84375000`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `path` is not \link RF_PATH_FILTER_BYPASS \endlink
///          and `lo_freq_hz > 5400000000`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `path` is not a valid \link rf_path_filter \endlink.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_freq_explicit(hackrf_device*      device,
                         uint64_t            if_freq_hz,
                         uint64_t            lo_freq_hz,
                         enum rf_path_filter path);

/// \brief FIXME: doc
///
/// currently 8-20Mhz - either as a fraction, i.e. freq 20000000hz divider 2 -> 10Mhz or as plain old 10000000hz (double)
/// preferred rates are 8, 10, 12.5, 16, 20Mhz due to less jitter
///
/// \param device  FIXME: doc
/// \param freq_hz FIXME: doc
/// \param divider FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_sample_rate_manual(hackrf_device* device,
                              uint32_t       freq_hz,
                              uint32_t       divider);

/// \brief FIXME: doc
///
/// \param device  FIXME: doc
/// \param freq_hz FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_sample_rate(hackrf_device* device,
                       double         freq_hz);

/// \brief FIXME: doc
///
/// external amp, bool on/off
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_amp_enable(hackrf_device* device,
                      uint8_t        value);

/// \brief FIXME: doc
///
/// range 0-40 step 8d, IF gain in osmosdr
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `value > 40`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_lna_gain(hackrf_device* device,
                    uint32_t       value);

/// \brief FIXME: doc
///
/// range 0-62 step 2db, BB gain in osmosdr
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `value > 62`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_vga_gain(hackrf_device* device,
                    uint32_t       value);

/// \brief FIXME: doc
///
/// range 0-47 step 1db
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `value > 47`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_txvga_gain(hackrf_device* device,
                      uint32_t       value);

/// \brief FIXME: doc
///
/// antenna port power control
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_antenna_enable(hackrf_device* device,
                          uint8_t        value);

/// \brief FIXME: doc
///
/// Compute nearest freq for bw filter (manual filter)
///
/// \param bandwidth_hz FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI uint32_t ADDCALL
hackrf_compute_baseband_filter_bw_round_down_lt(uint32_t bandwidth_hz);

/// \brief FIXME: doc
///
/// Compute best default value depending on sample rate (auto filter)
///
/// \param bandwidth_hz FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI uint32_t ADDCALL
hackrf_compute_baseband_filter_bw(uint32_t bandwidth_hz);

/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1002 or higher
///
/// set hardware sync mode
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1002`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_hw_sync_mode(hackrf_device* device,
                        uint8_t        value);

/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1002 or higher
///
/// Start sweep mode
///
/// \param device         FIXME: doc
/// \param frequency_list FIXME: doc
/// \param num_ranges     FIXME: doc
/// \param num_bytes      FIXME: doc
/// \param step_width     FIXME: doc
/// \param offset         FIXME: doc
/// \param style          FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `num_ranges < 1`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `num_ranges > MAX_SWEEP_RANGES`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `(num_samples % SAMPLES_PER_BLOCK) != 0`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `num_samples < SAMPLES_PER_BLOCK`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `step_width < 1`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `style` is not a valid \link sweep_style \endlink.
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1002`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_init_sweep(hackrf_device*   device,
                  const uint16_t*  frequency_list,
                  int              num_ranges,
                  uint32_t         num_bytes,
                  uint32_t         step_width,
                  uint32_t         offset,
                  enum sweep_style style);

// -----------------------------------------------------------------------------
// ---- Enum-to-string conversion functions ------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// \param errcode FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_error_name(enum hackrf_error errcode);

/// \brief FIXME: doc
///
/// \param board_id FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_board_id_name(enum hackrf_board_id board_id);

/// \brief FIXME: doc
///
/// \param usb_board_id FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_usb_board_id_name(enum hackrf_usb_board_id usb_board_id);

/// \brief FIXME: doc
///
/// \param path FIXME: doc
///
/// \returns FIXME: doc
extern ADDAPI const char* ADDCALL
hackrf_filter_path_name(enum rf_path_filter path);

// -----------------------------------------------------------------------------
// ---- Operacake functions ----------------------------------------------------
// -----------------------------------------------------------------------------

/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1002 or higher
///
/// \param device FIXME: doc
/// \param boards FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1002`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_get_operacake_boards(hackrf_device* device,
                            uint8_t*       boards);


/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1002 or higher
///
/// \param device  FIXME: doc
/// \param address FIXME: doc
/// \param port_a  FIXME: doc
/// \param port_b  FIXME: doc
///
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `port_a` is not a valid \link operacake_ports \endlink.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `port_b` is not a valid \link operacake_ports \endlink.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `port_a <= OPERACAKE_PA4` and `port_b <= OPERACAKE_PA4`.
/// \returns \link HACKRF_ERROR_INVALID_PARAM \endlink
///          if `port_a >= OPERACAKE_PB1` and `port_b >= OPERACAKE_PB1`.
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1002`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_operacake_ports(hackrf_device*       device,
                           uint8_t              address,
                           enum operacake_ports port_a,
                           enum operacake_ports port_b);


/// \brief FIXME: doc
///
/// This function requires HackRF USB API version 0x1003 or higher
///
/// \param device     FIXME: doc
/// \param ranges     FIXME: doc
/// \param num_ranges FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1003`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_operacake_ranges(hackrf_device* device,
                            uint8_t*       ranges,
                            uint8_t        num_ranges);

/// \brief FIXME: doc
///
/// FIXME: is this also an Operacake function?
///
/// This function requires HackRF USB API version 0x1002 or higher
///
/// \param device FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1002`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_reset(hackrf_device* device);

/// \brief FIXME: doc
///
/// FIXME: is this also an Operacake function?
///
/// This function requires HackRF USB API version 0x1003 or higher
///
/// \param device FIXME: doc
/// \param value  FIXME: doc
///
/// \returns \link HACKRF_ERROR_USB_API_VERSION \endlink
///          if the HackRF USB API version is lower than `0x1003`.
/// \returns \link HACKRF_ERROR_LIBUSB \endlink
///          if `libusb` had a problem.
/// \returns \link HACKRF_SUCCESS \endlink otherwise.
extern ADDAPI enum hackrf_error ADDCALL
hackrf_set_clkout_enable(hackrf_device* device,
                         uint8_t        value);

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#ifdef __cplusplus
} // __cplusplus defined.
#endif

#endif /*HACKRF_H*/

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
