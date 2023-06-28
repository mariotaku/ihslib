#include "ihslib/hid/hidapi.h"

#include "ihslib/hid.h"
#include "hid/provider.h"
#include "ihs_enumeration.h"

#include <hidapi.h>
#include <stdlib.h>

typedef struct HIDProviderImpl {
    IHS_HIDProvider base;
} HIDProviderImpl;

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls);

static void ProviderFree(IHS_HIDProvider *provider);

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path);

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path);

static bool ProviderHasChange(IHS_HIDProvider *provider);

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider);

static void ProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_HIDDeviceInfo *info);

static void *HidDeviceInfoNext(void *cur);

static const IHS_HIDProviderClass ProviderClass = {
        .alloc = ProviderAlloc,
        .free = ProviderFree,
        .supportsDevice = ProviderSupportsDevice,
        .openDevice = ProviderOpenDevice,
        .hasChange = ProviderHasChange,
        .enumerateDevices = ProviderEnumerate,
        .deviceInfo = ProviderDeviceInfo,
};

void IHS_HIDProviderHIDAPIInit() {
    hid_init();
}

void IHS_HIDProviderHIDAPIExit() {
    hid_exit();
}

IHS_HIDProvider *IHS_HIDProviderHIDAPICreate() {
    return IHS_SessionHIDProviderCreate(&ProviderClass);
}

void IHS_HIDProviderHIDAPIDestroy(IHS_HIDProvider *provider) {
    IHS_SessionHIDProviderDestroy(provider);
}

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls) {
    HIDProviderImpl *provider = calloc(1, sizeof(HIDProviderImpl));
    provider->base.cls = cls;
    return (IHS_HIDProvider *) provider;
}

static void ProviderFree(IHS_HIDProvider *provider) {
    free(provider);
}

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    return false;
}

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    return NULL;
}

static bool ProviderHasChange(IHS_HIDProvider *provider) {
    return false;
}

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider) {
    struct hid_device_info *info = hid_enumerate(0, 0);
    if (info == NULL) {
        return IHS_EnumerationEmptyCreate();
    }
    return IHS_EnumerationLinkedListCreate(info, HidDeviceInfoNext,
                                           (IHS_EnumerationFreeUnderlying) hid_free_enumeration);
}

static void ProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_HIDDeviceInfo *info) {
    struct hid_device_info *cur = IHS_EnumerationGet(enumeration);
    info->path = cur->path;
    info->product_string = (char *) cur->product_string;
    info->vendor_id = cur->vendor_id;
    info->product_id = cur->product_id;
    info->product_version = cur->release_number;
}

static void *HidDeviceInfoNext(void *cur) {
    return ((struct hid_device_info *) cur)->next;
}