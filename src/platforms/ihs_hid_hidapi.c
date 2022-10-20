#include <hidapi.h>

#include "ihslib/hid.h"
#include "ihs_buffer.h"

static int EnumerationLength(IHS_StreamHIDDeviceEnumeration *devices, void *context);

static IHS_StreamHIDDeviceEnumeration *EnumerationNext(IHS_StreamHIDDeviceEnumeration *devices, void *context);

static void EnumerationInfo(const IHS_StreamHIDDeviceEnumeration *enumeration, IHS_StreamHIDDeviceInfo *info,
                            void *context);

const static IHS_StreamHIDInterface HIDAPIInterface = {
        .enumerate = (void *) hid_enumerate,
        .enumeration_length = EnumerationLength,
        .enumeration_next = EnumerationNext,
        .enumeration_getinfo = EnumerationInfo,
        .free_enumeration = (void *) hid_free_enumeration,
        .open = (void *) hid_open,
        .open_path = (void *) hid_open_path,
        .write = (void *) hid_write,
        .read_timeout = (void *) hid_read_timeout,
        .read = (void *) hid_read,
        .set_nonblocking = (void *) hid_set_nonblocking,
        .send_feature_report = (void *) hid_send_feature_report,
        .get_feature_report = (void *) hid_get_feature_report,
        .close = (void *) hid_close,
};

static int EnumerationLength(IHS_StreamHIDDeviceEnumeration *devices, void *context) {
    (void) context;
    int length = 0;
    for (struct hid_device_info *cur = (struct hid_device_info *) devices; cur != NULL; cur = cur->next) {
        length++;
    }
    return length;
}

static IHS_StreamHIDDeviceEnumeration *EnumerationNext(IHS_StreamHIDDeviceEnumeration *devices, void *context) {
    (void) context;
    return (IHS_StreamHIDDeviceEnumeration *) ((struct hid_device_info *) devices)->next;
}

static void EnumerationInfo(const IHS_StreamHIDDeviceEnumeration *enumeration, IHS_StreamHIDDeviceInfo *info,
                            void *context) {
    (void) context;
    const struct hid_device_info *hid = (const struct hid_device_info *) enumeration;
    info->path = hid->path;
    info->vendor_id = hid->vendor_id;
    info->product_id = hid->product_id;
    info->serial_number = hid->serial_number;
    info->release_number = hid->release_number;
    info->manufacturer_string = hid->manufacturer_string;
    info->product_string = hid->product_string;
    info->usage_page = hid->usage_page;
    info->usage = hid->usage;
    info->interface_number = hid->interface_number;
}

const IHS_StreamHIDInterface *IHS_StreamHIDInterfaceHIDAPI() {
    return &HIDAPIInterface;
}
