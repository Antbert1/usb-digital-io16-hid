/** $$$$$$$$$ WIP $$$$$$
* USB HID I/O DEVICE API library
* http://git.io/hx9J
*
* This is reconstruction of the original Windows DLL, as provided by the vendor.
* It is binary compatible and works with their example programs.
* The original .h file has been slightly hacked up.
*
* 14-apr-2015 pa01 Windows version
*/

#define MY_VERSION 0x02

#if defined (WIN32) || defined (_WIN32)
// Windows 32 or 64 bit
#include "targetver.h"
#define WIN32_EXTRALEAN
#include <windows.h>

#ifdef _MSC_VER
/* The original DLL has cdecl calling convention */
#define USBRL_CALL __cdecl
#define USBRL_API __declspec(dllexport) USBRL_CALL
#endif // _MSC_VER

#else  //WIN32
#define USBRL_API /**/
#endif //WIN32

#include "usb_io_device.h"

#if defined(USBIO16_LIB_VER) && (USBIO16_LIB_VER != MY_VERSION)
#error "Oops. Wrong version of usb_io_device.h"
#endif

#define EXPORT_API USBRL_API

#if _MSC_VER < 1900 /* before VS2015 */
#define snprintf   _snprintf
#endif /* VS2015 */

#include "usb_io_hw.h"
#include "hiddata.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if 1
//#define dbgprintf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define dbgprintf(fmt, ...) printf(fmt, __VA_ARGS__)
//#define dbgprintf(fmt, ...) __noop(fmt, __VA_ARGS__)
//#define printerr(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define printerr(fmt, ...) printf(fmt, __VA_ARGS__)
//#define printerr(fmt, ...) __noop(fmt, __VA_ARGS__)
#else
#define dbgprintf(fmt, ...) do;while(0)
#define printerr(fmt, ...) do;while(0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//typedef struct usb_io_device_info * pusb_io_device_info_t;

struct usb_io_internal_s {
    struct usb_io_device_info urdi; //public part
    // Private part:
    USBDEVHANDLE usbh; // handle
    int type;
    char idstr[USB_IO16_ID_STR_LEN + 1];
};

// struct for enum context
struct enumctx_s {
    struct usb_io_internal_s *head, *tail;
    int numdevs;
    int status;
};

// Globals

const char *g_dummyPath = "NOTHING"; // passing the dev.path to client not implemented, we return this as path.

static const char *usbErrorMessage(int errCode)
{
    static char buffer[80];
    buffer[0] = 0;
    if ( errCode != USBHID_ERR_UNKNOWN ) {
        usbhidStrerror_r(errCode, buffer, sizeof(buffer));
    }
    if ( 0 == buffer[0] ) {
        snprintf(buffer, sizeof(buffer), "Unknown error (%d)", errCode);
    }
    return buffer;
}


// Read HID report
static int d16_read_rep(USBDEVHANDLE dev, int reportnum, unsigned char raw_data[8+1])
{
    char buffer[10];
    int err;
    int len = 8 + 1; /* report id 1 byte + 8 bytes data */
    memset(buffer, 0, sizeof(buffer));

    err = usbhidGetReport(dev, reportnum, buffer, &len);
    if ( err ) {
        printerr("error reading hid report: %s\n", usbErrorMessage(err));
        return -1;
    }

    if ( len != 9 || buffer[0] != reportnum ) {
        printerr("ERROR: wrong HID report returned! %d\n", len);
        return -2;
    }

    if ( raw_data ) {
        /* copy raw report data */
        memcpy(raw_data, buffer, len);
    }

    return 0;
}


// Write HID report
static int d16_write_rep(USBDEVHANDLE dev, int reportnum, unsigned char buffer[8+1])
{
    int err;
    int len = 8 + 1; /* report id 1 byte + 8 bytes data */

    buffer[0] = (unsigned char)reportnum;
    err = usbhidSetReport(dev, buffer, len);
    if ( err ) {
        printerr("error writing hid report: %s\n", usbErrorMessage(err));
        return -1;
    }

    return err;
}

// Enum function for building list of devices
static
int enumfunc(USBDEVHANDLE usbh, void *context)
{
    static const char vendorName[] = USB_IO16_VENDOR_NAME;
    static const char productName[] = USB_IO16_NAME;
    int err;
    char buffer[128 * sizeof(short)]; // max USB string is 128 UTF-16 chars
    int i;
    struct usb_io_internal_s *q;
    struct enumctx_s *ectx = (struct enumctx_s *)context;

    err = usbhidGetVendorString(usbh, buffer, sizeof(buffer));
    if ( err )
    {
        goto next;
    }

    if ( 0 != strcmp(buffer, vendorName) )
    {
        goto next;
    }

    err = usbhidGetProductString(usbh, buffer, sizeof(buffer));
    if ( err )
    {
        goto next;
    }

    if ( 0 != strcmp(buffer, productName) )
    {
        goto next;
    }

    /* Check the unique ID: USB_IO16_ID_STR_LEN  bytes at offset 1 (just after the report id) */
    err = d16_read_rep(usbh, 0, buffer);
    if ( err < 0 )
    {
        dbgprintf("Error reading report 0: %s\n", usbErrorMessage(err));
        goto next;
    }

    for ( i = 1; i <= USB_IO16_ID_STR_LEN; i++ )
    {
        unsigned char x = (unsigned char)buffer[i];
        if ( x <= 0x20 || x >= 0x7F )
        {
            dbgprintf("Bad USBIO16 ID string!\n");
            goto next;
        }
    }

    dbgprintf("Device %s found: ID=[%.*s]\n", productName, USB_IO16_ID_STR_LEN, &buffer[1]);

    // allocate & save info
    q = (struct usb_io_internal_s *)calloc(1, sizeof(struct usb_io_internal_s));
    if ( !q ) {
        dbgprintf("Malloc err\n");
        goto next; //$$$ revise
    }
    /* keep this device, continue */
    q->usbh = usbh;
    memcpy(q->idstr, &buffer[1], USB_IO16_ID_STR_LEN);
    q->type = 16; //  $$$ # pins
    q->urdi.serial_number = &q->idstr[0];
    q->urdi.device_path = (char*)g_dummyPath;

    if ( !ectx->head ) {
        ectx->head = q;
        ectx->tail = q;
    } else {
        ectx->tail->urdi.next = (pusb_io_device_info_t)q;
    }

    ++ectx->numdevs;
    return 1;

next:
    /* Continue search */
    usbhidCloseDevice(usbh);
    return 1;
}


// Public functions:

/** Initialize the Library
@returns: This function returns 0 on success and -1 on error.
*/
int EXPORT_API usb_io_init(void)
{
    return 0;
}

/** Finalize the Library.
This function frees all of the static data associated with Library.
It should be called at the end of execution to avoid memory leaks.
@returns: This function returns 0 on success and -1 on error.
*/
int EXPORT_API usb_io_uninit(void)
{
    return 0;
}


/** Enumerate the devices.*/
pusb_io_device_info_t EXPORT_API usb_io_get_device_list(void)
{
    struct enumctx_s ectx;
    int ret;
    memset(&ectx, 0, sizeof(ectx));
    ret = usbhidEnumDevices(USB_CFG_VENDOR_ID, USB_CFG_DEVICE_ID,
        (void*)&ectx,
        enumfunc);

    return (pusb_io_device_info_t)ectx.head;
}


/** Free an enumeration Linked List*/
void USBRL_API usb_io_free_device_list(struct usb_io_device_info *dilist)
{
    struct usb_io_internal_s *p = (struct usb_io_internal_s *)dilist;

    while ( p ) {
        struct usb_io_internal_s *q = (struct usb_io_internal_s *)((pusb_io_device_info_t)p)->next;
        if ( p->usbh && ((USBDEVHANDLE)(-1)) != p->usbh ) {
            usbhidCloseDevice(p->usbh);
            p->usbh = 0;
        }
        free(p);
        p = q;
    }

    return;
}


#if 0 // not in orig API - do we want it?

// Enum function for open one device by ID
static
    int enumOpenfunc(USBDEVHANDLE usbh, void *context)
{
    static const char vendorName[]  = USB_IO16_VENDOR_NAME;
    static const char productName[] = USB_IO16_NAME;
    int err;
    char buffer[128 * sizeof(short)]; // max USB string is 128 UTF-16 chars
    int i;
    struct enumctx_s *ectx = (struct enumctx_s *)context;
    struct usb_io_internal_s *q = ectx->head;


    err = usbhidGetVendorString(usbh, buffer, sizeof(buffer));
    if ( err )
    {
        goto next;
    }

    if ( 0 != strcmp(buffer, vendorName) )
    {
        goto next;
    }

    err = usbhidGetProductString(usbh, buffer, sizeof(buffer));
    if ( err )
    {
        goto next;
    }

    if ( 0 != strcmp(buffer, productName) )
    {
        goto next;
    }

    /* Check the unique ID: USB_IO16_ID_STR_LEN bytes at offset 1 (just after the report id) */
    err = rel_read_status_raw(usbh, buffer);
    if ( err < 0 )
    {
        dbgprintf("Error reading report 0: %s\n", usbErrorMessage(err));
        goto next;
    }

    for ( i = 1; i <= USB_IO16_ID_STR_LEN; i++ )
    {
        unsigned char x = (unsigned char)buffer[i];
        if ( x <= 0x20 || x >= 0x7F )
        {
            dbgprintf("Bad USBIO16 ID string!\n");
            goto next;
        }
    }

    dbgprintf("Device %s found: ID=[%*s]\n", USB_IO16_ID_STR_LEN, productName, &buffer[1]);

    if ( 0 == memcmp(q->idstr, &buffer[1], USB_IO16_ID_STR_LEN) ) {
        q->usbh = usbh;
        q->type = 16;  // *** # of pins
        q->urdi.serial_number = &q->idstr[0];
        q->urdi.device_path = (char*)g_dummyPath;
        ++ectx->numdevs;
        return 0;
    }

next:
    /* Continue search */
    usbhidCloseDevice(usbh);
    return 1;
}


/** Open device by serial number
serial_number == NULL is valid and means any one device.
@return: This function returns a valid handle to the device on success or NULL on failure.
Example: usb_relay_device_open_with_serial_number("abcde", 5)
*/
intptr_t USBRL_API usb_io_device_open_with_serial_number(const char *serial_number, unsigned len)
{
    struct enumctx_s ectx;
    int ret;
    struct usbrelay_internal_s *q;
    memset(&ectx, 0, sizeof(ectx));

    if ( serial_number && len != USB_RELAY_ID_STR_LEN ) {
        printerr("Specified invalid str id length: %u", len);
        return (intptr_t)0;
    }

    q = ectx.head = calloc(1, sizeof(*ectx.head));
    if ( !q )
        return (intptr_t)0;

    memcpy(q->idstr, serial_number, len);

    ret = usbhidEnumDevices(USB_CFG_VENDOR_ID, USB_CFG_DEVICE_ID,
        (void*)&ectx,
        enumOpenfunc);
    if ( ret != 0 )
        goto ret_err; // error during enum

    if ( ectx.numdevs == 0 || q->usbh == 0 ) {
        goto ret_err; // not found
    }

    q->urdi.next = (void*)q; // mark this element as standalone
    return (intptr_t)q;

    ret_err:
    free(q);
    return (intptr_t)0;
}
#endif //-------------

/** Open a  device
@return: This function returns a valid handle to the device on success or NULL on failure.
*/
intptr_t EXPORT_API usb_io_open_device(struct usb_io_device_info *device_info)
{
    struct usb_io_internal_s *p = (struct usb_io_internal_s *)device_info;
    if ( !device_info )
        return 0;
    if ( (uintptr_t)p->usbh == 0 || (uintptr_t)p->usbh == (uintptr_t)-1 )
        return 0;
    //$$$ validate more
    return (uintptr_t)device_info;
}

/** Close a device*/
void EXPORT_API usb_io_close_device(intptr_t hHandle)
{
    struct usb_io_internal_s *p = (struct usb_io_internal_s *)hHandle;
    if ( 0 == hHandle || ((intptr_t)-1) == hHandle )
        return;

    if ( (void*)(p->urdi.next) == (void*)p ) {
        // This was made by usb_xxxx_device_open_with_serial_number() so free it now:
        if ( p->usbh && ((intptr_t)-1) != (intptr_t)(p->usbh) ) {
            usbhidCloseDevice(p->usbh);
            p->usbh = 0;
        }
        p->urdi.next = NULL;
        free((void*)p);
    }
    // Else this can be in the live list, don't do anything.
}


int EXPORT_API usb_io_set_work_led_mode(intptr_t hHandle, enum work_led_mode led_mode)
{
    unsigned char buf[9];
    struct usb_io_internal_s *p = (struct usb_io_internal_s *)hHandle;

    memset(buf, 0, sizeof(buf));
    buf[1] = 0xF8;
    if (led_mode) {
        buf[2] = 1;
    }

    return d16_write_rep(p->usbh, 0, buf);
}

int EXPORT_API usb_io_set_pin_mode(intptr_t hHandle,
    unsigned pinIndex,
    enum pin_mode mode,
    enum input_pin_mode innerPullUp)
{
    return -1;
}

int EXPORT_API usb_io_write_output_pin_value(intptr_t hHandle, unsigned ouputPinIndex, enum pin_level level)
{
    return -1;
}


int EXPORT_API usb_io_read_input_pin_value(intptr_t hHandle, unsigned pinIndex, unsigned *level)
{
    return -1;
}

int EXPORT_API usb_io_get_all_pin_info(intptr_t hHandle, struct pin_info info[USB_IO16_MAX_PIN_NUM])
{
    return -1;
}


/************ Added ****************/

/** Get the library (dll) version
@return Lower 16 bits: the library version. Higher bits: undefined, ignore.
@note The original DLL does not have this function!
*/
int EXPORT_API usb_io16_lib_version(void)
{
    return (int)(MY_VERSION);
}

/**
The following functions are for non-native callers, to avoid fumbling with C structs.
Native C/C++ callers do not need to use these.
The ptr_usb_relay_device_info arg is pointer to struct usb_relay_device_info, cast to intptr_t, void*, etc.
*/

/* Return next info struct pointer in the list returned by usb_relay_device_enumerate() */
intptr_t EXPORT_API usb_io_device_next_dev(intptr_t ptr_device_info)
{
    if ( !ptr_device_info )
        return 0;
    return (intptr_t)(void*)((pusb_io_device_info_t)ptr_device_info)->next;
}


/* Get the ID string of the device. Returns pointer to const C string (1-byte, 0-terminated) */
intptr_t EXPORT_API usb_io_device_get_id_string(intptr_t ptr_device_info)
{
    if ( !ptr_device_info )
        return 0;
    return (intptr_t)(void const *)((pusb_io_device_info_t)ptr_device_info)->serial_number;
}

#ifdef __cplusplus
}
#endif
