/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: protobuf/hiddevices.proto */

#ifndef PROTOBUF_C_protobuf_2fhiddevices_2eproto__INCLUDED
#define PROTOBUF_C_protobuf_2fhiddevices_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1003003 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _CHIDDeviceInfo CHIDDeviceInfo;
typedef struct _CHIDDeviceInputReport CHIDDeviceInputReport;
typedef struct _CHIDMessageToRemote CHIDMessageToRemote;
typedef struct _CHIDMessageToRemote__DeviceOpen CHIDMessageToRemote__DeviceOpen;
typedef struct _CHIDMessageToRemote__DeviceClose CHIDMessageToRemote__DeviceClose;
typedef struct _CHIDMessageToRemote__DeviceWrite CHIDMessageToRemote__DeviceWrite;
typedef struct _CHIDMessageToRemote__DeviceRead CHIDMessageToRemote__DeviceRead;
typedef struct _CHIDMessageToRemote__DeviceSendFeatureReport CHIDMessageToRemote__DeviceSendFeatureReport;
typedef struct _CHIDMessageToRemote__DeviceGetFeatureReport CHIDMessageToRemote__DeviceGetFeatureReport;
typedef struct _CHIDMessageToRemote__DeviceGetVendorString CHIDMessageToRemote__DeviceGetVendorString;
typedef struct _CHIDMessageToRemote__DeviceGetProductString CHIDMessageToRemote__DeviceGetProductString;
typedef struct _CHIDMessageToRemote__DeviceGetSerialNumberString CHIDMessageToRemote__DeviceGetSerialNumberString;
typedef struct _CHIDMessageToRemote__DeviceStartInputReports CHIDMessageToRemote__DeviceStartInputReports;
typedef struct _CHIDMessageToRemote__DeviceRequestFullReport CHIDMessageToRemote__DeviceRequestFullReport;
typedef struct _CHIDMessageToRemote__DeviceDisconnect CHIDMessageToRemote__DeviceDisconnect;
typedef struct _CHIDMessageFromRemote CHIDMessageFromRemote;
typedef struct _CHIDMessageFromRemote__UpdateDeviceList CHIDMessageFromRemote__UpdateDeviceList;
typedef struct _CHIDMessageFromRemote__RequestResponse CHIDMessageFromRemote__RequestResponse;
typedef struct _CHIDMessageFromRemote__DeviceInputReports CHIDMessageFromRemote__DeviceInputReports;
typedef struct _CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport;
typedef struct _CHIDMessageFromRemote__CloseDevice CHIDMessageFromRemote__CloseDevice;
typedef struct _CHIDMessageFromRemote__CloseAllDevices CHIDMessageFromRemote__CloseAllDevices;


/* --- enums --- */

typedef enum _EHIDDeviceLocation {
  k_EDeviceLocationLocal = 0,
  k_EDeviceLocationRemote = 2,
  k_EDeviceLocationAny = 3
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(EHIDDEVICE_LOCATION)
} EHIDDeviceLocation;
typedef enum _EHIDDeviceDisconnectMethod {
  k_EDeviceDisconnectMethodUnknown = 0,
  k_EDeviceDisconnectMethodBluetooth = 1,
  k_EDeviceDisconnectMethodFeatureReport = 2,
  k_EDeviceDisconnectMethodOutputReport = 3
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(EHIDDEVICE_DISCONNECT_METHOD)
} EHIDDeviceDisconnectMethod;

/* --- messages --- */

struct  _CHIDDeviceInfo
{
  ProtobufCMessage base;
  protobuf_c_boolean has_location;
  EHIDDeviceLocation location;
  char *path;
  protobuf_c_boolean has_vendor_id;
  uint32_t vendor_id;
  protobuf_c_boolean has_product_id;
  uint32_t product_id;
  char *serial_number;
  protobuf_c_boolean has_release_number;
  uint32_t release_number;
  char *manufacturer_string;
  char *product_string;
  protobuf_c_boolean has_usage_page;
  uint32_t usage_page;
  protobuf_c_boolean has_usage;
  uint32_t usage;
  protobuf_c_boolean has_interface_number;
  int32_t interface_number;
  protobuf_c_boolean has_ostype;
  int32_t ostype;
  protobuf_c_boolean has_is_generic_gamepad;
  protobuf_c_boolean is_generic_gamepad;
  protobuf_c_boolean has_is_generic_joystick;
  protobuf_c_boolean is_generic_joystick;
  protobuf_c_boolean has_caps_bits;
  uint32_t caps_bits;
  protobuf_c_boolean has_session_id;
  uint32_t session_id;
  protobuf_c_boolean has_econtrollertype_obsolete;
  uint32_t econtrollertype_obsolete;
  protobuf_c_boolean has_is_xinput_device_obsolete;
  protobuf_c_boolean is_xinput_device_obsolete;
  protobuf_c_boolean has_session_remote_play_together_appid;
  uint32_t session_remote_play_together_appid;
};
#define CHIDDEVICE_INFO__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chiddevice_info__descriptor) \
    , 0, k_EDeviceLocationLocal, NULL, 0, 0, 0, 0, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0u, 0, 0, 0, 0 }


struct  _CHIDDeviceInputReport
{
  ProtobufCMessage base;
  protobuf_c_boolean has_full_report;
  ProtobufCBinaryData full_report;
  protobuf_c_boolean has_delta_report;
  ProtobufCBinaryData delta_report;
  protobuf_c_boolean has_delta_report_size;
  uint32_t delta_report_size;
  protobuf_c_boolean has_delta_report_crc;
  uint32_t delta_report_crc;
};
#define CHIDDEVICE_INPUT_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chiddevice_input_report__descriptor) \
    , 0, {0,NULL}, 0, {0,NULL}, 0, 0, 0, 0 }


struct  _CHIDMessageToRemote__DeviceOpen
{
  ProtobufCMessage base;
  CHIDDeviceInfo *info;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_OPEN__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_open__descriptor) \
    , NULL }


struct  _CHIDMessageToRemote__DeviceClose
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_CLOSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_close__descriptor) \
    , 0, 0 }


struct  _CHIDMessageToRemote__DeviceWrite
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_data;
  ProtobufCBinaryData data;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_WRITE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_write__descriptor) \
    , 0, 0, 0, {0,NULL} }


struct  _CHIDMessageToRemote__DeviceRead
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_length;
  uint32_t length;
  protobuf_c_boolean has_timeout_ms;
  int32_t timeout_ms;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_READ__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_read__descriptor) \
    , 0, 0, 0, 0, 0, 0 }


struct  _CHIDMessageToRemote__DeviceSendFeatureReport
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_data;
  ProtobufCBinaryData data;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_SEND_FEATURE_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_send_feature_report__descriptor) \
    , 0, 0, 0, {0,NULL} }


struct  _CHIDMessageToRemote__DeviceGetFeatureReport
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_report_number;
  ProtobufCBinaryData report_number;
  protobuf_c_boolean has_length;
  uint32_t length;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_GET_FEATURE_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_get_feature_report__descriptor) \
    , 0, 0, 0, {0,NULL}, 0, 0 }


struct  _CHIDMessageToRemote__DeviceGetVendorString
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_GET_VENDOR_STRING__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_get_vendor_string__descriptor) \
    , 0, 0 }


struct  _CHIDMessageToRemote__DeviceGetProductString
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_GET_PRODUCT_STRING__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_get_product_string__descriptor) \
    , 0, 0 }


struct  _CHIDMessageToRemote__DeviceGetSerialNumberString
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_GET_SERIAL_NUMBER_STRING__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_get_serial_number_string__descriptor) \
    , 0, 0 }


struct  _CHIDMessageToRemote__DeviceStartInputReports
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_length;
  uint32_t length;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_START_INPUT_REPORTS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_start_input_reports__descriptor) \
    , 0, 0, 0, 0 }


struct  _CHIDMessageToRemote__DeviceRequestFullReport
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_REQUEST_FULL_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_request_full_report__descriptor) \
    , 0, 0 }


struct  _CHIDMessageToRemote__DeviceDisconnect
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  protobuf_c_boolean has_disconnectmethod;
  EHIDDeviceDisconnectMethod disconnectmethod;
  protobuf_c_boolean has_data;
  ProtobufCBinaryData data;
};
#define CHIDMESSAGE_TO_REMOTE__DEVICE_DISCONNECT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__device_disconnect__descriptor) \
    , 0, 0, 0, k_EDeviceDisconnectMethodUnknown, 0, {0,NULL} }


typedef enum {
  CHIDMESSAGE_TO_REMOTE__COMMAND__NOT_SET = 0,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_OPEN = 2,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_CLOSE = 3,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_WRITE = 4,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_READ = 5,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_SEND_FEATURE_REPORT = 6,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_FEATURE_REPORT = 7,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_VENDOR_STRING = 8,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_PRODUCT_STRING = 9,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_SERIAL_NUMBER_STRING = 10,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_START_INPUT_REPORTS = 11,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_REQUEST_FULL_REPORT = 12,
  CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_DISCONNECT = 13
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(CHIDMESSAGE_TO_REMOTE__COMMAND)
} CHIDMessageToRemote__CommandCase;

struct  _CHIDMessageToRemote
{
  ProtobufCMessage base;
  protobuf_c_boolean has_request_id;
  uint32_t request_id;
  CHIDMessageToRemote__CommandCase command_case;
  union {
    CHIDMessageToRemote__DeviceOpen *device_open;
    CHIDMessageToRemote__DeviceClose *device_close;
    CHIDMessageToRemote__DeviceWrite *device_write;
    CHIDMessageToRemote__DeviceRead *device_read;
    CHIDMessageToRemote__DeviceSendFeatureReport *device_send_feature_report;
    CHIDMessageToRemote__DeviceGetFeatureReport *device_get_feature_report;
    CHIDMessageToRemote__DeviceGetVendorString *device_get_vendor_string;
    CHIDMessageToRemote__DeviceGetProductString *device_get_product_string;
    CHIDMessageToRemote__DeviceGetSerialNumberString *device_get_serial_number_string;
    CHIDMessageToRemote__DeviceStartInputReports *device_start_input_reports;
    CHIDMessageToRemote__DeviceRequestFullReport *device_request_full_report;
    CHIDMessageToRemote__DeviceDisconnect *device_disconnect;
  };
};
#define CHIDMESSAGE_TO_REMOTE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_to_remote__descriptor) \
    , 0, 0, CHIDMESSAGE_TO_REMOTE__COMMAND__NOT_SET, {0} }


struct  _CHIDMessageFromRemote__UpdateDeviceList
{
  ProtobufCMessage base;
  size_t n_devices;
  CHIDDeviceInfo **devices;
};
#define CHIDMESSAGE_FROM_REMOTE__UPDATE_DEVICE_LIST__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__update_device_list__descriptor) \
    , 0,NULL }


struct  _CHIDMessageFromRemote__RequestResponse
{
  ProtobufCMessage base;
  protobuf_c_boolean has_request_id;
  uint32_t request_id;
  protobuf_c_boolean has_result;
  int32_t result;
  protobuf_c_boolean has_data;
  ProtobufCBinaryData data;
};
#define CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__request_response__descriptor) \
    , 0, 0, 0, 0, 0, {0,NULL} }


struct  _CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
  size_t n_reports;
  CHIDDeviceInputReport **reports;
};
#define CHIDMESSAGE_FROM_REMOTE__DEVICE_INPUT_REPORTS__DEVICE_INPUT_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__device_input_reports__device_input_report__descriptor) \
    , 0, 0, 0,NULL }


struct  _CHIDMessageFromRemote__DeviceInputReports
{
  ProtobufCMessage base;
  size_t n_device_reports;
  CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport **device_reports;
};
#define CHIDMESSAGE_FROM_REMOTE__DEVICE_INPUT_REPORTS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__device_input_reports__descriptor) \
    , 0,NULL }


struct  _CHIDMessageFromRemote__CloseDevice
{
  ProtobufCMessage base;
  protobuf_c_boolean has_device;
  uint32_t device;
};
#define CHIDMESSAGE_FROM_REMOTE__CLOSE_DEVICE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__close_device__descriptor) \
    , 0, 0 }


struct  _CHIDMessageFromRemote__CloseAllDevices
{
  ProtobufCMessage base;
};
#define CHIDMESSAGE_FROM_REMOTE__CLOSE_ALL_DEVICES__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__close_all_devices__descriptor) \
     }


typedef enum {
  CHIDMESSAGE_FROM_REMOTE__COMMAND__NOT_SET = 0,
  CHIDMESSAGE_FROM_REMOTE__COMMAND_UPDATE_DEVICE_LIST = 1,
  CHIDMESSAGE_FROM_REMOTE__COMMAND_RESPONSE = 2,
  CHIDMESSAGE_FROM_REMOTE__COMMAND_REPORTS = 3,
  CHIDMESSAGE_FROM_REMOTE__COMMAND_CLOSE_DEVICE = 4,
  CHIDMESSAGE_FROM_REMOTE__COMMAND_CLOSE_ALL_DEVICES = 5
    PROTOBUF_C__FORCE_ENUM_TO_BE_INT_SIZE(CHIDMESSAGE_FROM_REMOTE__COMMAND)
} CHIDMessageFromRemote__CommandCase;

struct  _CHIDMessageFromRemote
{
  ProtobufCMessage base;
  CHIDMessageFromRemote__CommandCase command_case;
  union {
    CHIDMessageFromRemote__UpdateDeviceList *update_device_list;
    CHIDMessageFromRemote__RequestResponse *response;
    CHIDMessageFromRemote__DeviceInputReports *reports;
    CHIDMessageFromRemote__CloseDevice *close_device;
    CHIDMessageFromRemote__CloseAllDevices *close_all_devices;
  };
};
#define CHIDMESSAGE_FROM_REMOTE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&chidmessage_from_remote__descriptor) \
    , CHIDMESSAGE_FROM_REMOTE__COMMAND__NOT_SET, {0} }


/* CHIDDeviceInfo methods */
void   chiddevice_info__init
                     (CHIDDeviceInfo         *message);
size_t chiddevice_info__get_packed_size
                     (const CHIDDeviceInfo   *message);
size_t chiddevice_info__pack
                     (const CHIDDeviceInfo   *message,
                      uint8_t             *out);
size_t chiddevice_info__pack_to_buffer
                     (const CHIDDeviceInfo   *message,
                      ProtobufCBuffer     *buffer);
CHIDDeviceInfo *
       chiddevice_info__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   chiddevice_info__free_unpacked
                     (CHIDDeviceInfo *message,
                      ProtobufCAllocator *allocator);
/* CHIDDeviceInputReport methods */
void   chiddevice_input_report__init
                     (CHIDDeviceInputReport         *message);
size_t chiddevice_input_report__get_packed_size
                     (const CHIDDeviceInputReport   *message);
size_t chiddevice_input_report__pack
                     (const CHIDDeviceInputReport   *message,
                      uint8_t             *out);
size_t chiddevice_input_report__pack_to_buffer
                     (const CHIDDeviceInputReport   *message,
                      ProtobufCBuffer     *buffer);
CHIDDeviceInputReport *
       chiddevice_input_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   chiddevice_input_report__free_unpacked
                     (CHIDDeviceInputReport *message,
                      ProtobufCAllocator *allocator);
/* CHIDMessageToRemote__DeviceOpen methods */
void   chidmessage_to_remote__device_open__init
                     (CHIDMessageToRemote__DeviceOpen         *message);
/* CHIDMessageToRemote__DeviceClose methods */
void   chidmessage_to_remote__device_close__init
                     (CHIDMessageToRemote__DeviceClose         *message);
/* CHIDMessageToRemote__DeviceWrite methods */
void   chidmessage_to_remote__device_write__init
                     (CHIDMessageToRemote__DeviceWrite         *message);
/* CHIDMessageToRemote__DeviceRead methods */
void   chidmessage_to_remote__device_read__init
                     (CHIDMessageToRemote__DeviceRead         *message);
/* CHIDMessageToRemote__DeviceSendFeatureReport methods */
void   chidmessage_to_remote__device_send_feature_report__init
                     (CHIDMessageToRemote__DeviceSendFeatureReport         *message);
/* CHIDMessageToRemote__DeviceGetFeatureReport methods */
void   chidmessage_to_remote__device_get_feature_report__init
                     (CHIDMessageToRemote__DeviceGetFeatureReport         *message);
/* CHIDMessageToRemote__DeviceGetVendorString methods */
void   chidmessage_to_remote__device_get_vendor_string__init
                     (CHIDMessageToRemote__DeviceGetVendorString         *message);
/* CHIDMessageToRemote__DeviceGetProductString methods */
void   chidmessage_to_remote__device_get_product_string__init
                     (CHIDMessageToRemote__DeviceGetProductString         *message);
/* CHIDMessageToRemote__DeviceGetSerialNumberString methods */
void   chidmessage_to_remote__device_get_serial_number_string__init
                     (CHIDMessageToRemote__DeviceGetSerialNumberString         *message);
/* CHIDMessageToRemote__DeviceStartInputReports methods */
void   chidmessage_to_remote__device_start_input_reports__init
                     (CHIDMessageToRemote__DeviceStartInputReports         *message);
/* CHIDMessageToRemote__DeviceRequestFullReport methods */
void   chidmessage_to_remote__device_request_full_report__init
                     (CHIDMessageToRemote__DeviceRequestFullReport         *message);
/* CHIDMessageToRemote__DeviceDisconnect methods */
void   chidmessage_to_remote__device_disconnect__init
                     (CHIDMessageToRemote__DeviceDisconnect         *message);
/* CHIDMessageToRemote methods */
void   chidmessage_to_remote__init
                     (CHIDMessageToRemote         *message);
size_t chidmessage_to_remote__get_packed_size
                     (const CHIDMessageToRemote   *message);
size_t chidmessage_to_remote__pack
                     (const CHIDMessageToRemote   *message,
                      uint8_t             *out);
size_t chidmessage_to_remote__pack_to_buffer
                     (const CHIDMessageToRemote   *message,
                      ProtobufCBuffer     *buffer);
CHIDMessageToRemote *
       chidmessage_to_remote__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   chidmessage_to_remote__free_unpacked
                     (CHIDMessageToRemote *message,
                      ProtobufCAllocator *allocator);
/* CHIDMessageFromRemote__UpdateDeviceList methods */
void   chidmessage_from_remote__update_device_list__init
                     (CHIDMessageFromRemote__UpdateDeviceList         *message);
/* CHIDMessageFromRemote__RequestResponse methods */
void   chidmessage_from_remote__request_response__init
                     (CHIDMessageFromRemote__RequestResponse         *message);
/* CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport methods */
void   chidmessage_from_remote__device_input_reports__device_input_report__init
                     (CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport         *message);
/* CHIDMessageFromRemote__DeviceInputReports methods */
void   chidmessage_from_remote__device_input_reports__init
                     (CHIDMessageFromRemote__DeviceInputReports         *message);
/* CHIDMessageFromRemote__CloseDevice methods */
void   chidmessage_from_remote__close_device__init
                     (CHIDMessageFromRemote__CloseDevice         *message);
/* CHIDMessageFromRemote__CloseAllDevices methods */
void   chidmessage_from_remote__close_all_devices__init
                     (CHIDMessageFromRemote__CloseAllDevices         *message);
/* CHIDMessageFromRemote methods */
void   chidmessage_from_remote__init
                     (CHIDMessageFromRemote         *message);
size_t chidmessage_from_remote__get_packed_size
                     (const CHIDMessageFromRemote   *message);
size_t chidmessage_from_remote__pack
                     (const CHIDMessageFromRemote   *message,
                      uint8_t             *out);
size_t chidmessage_from_remote__pack_to_buffer
                     (const CHIDMessageFromRemote   *message,
                      ProtobufCBuffer     *buffer);
CHIDMessageFromRemote *
       chidmessage_from_remote__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   chidmessage_from_remote__free_unpacked
                     (CHIDMessageFromRemote *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*CHIDDeviceInfo_Closure)
                 (const CHIDDeviceInfo *message,
                  void *closure_data);
typedef void (*CHIDDeviceInputReport_Closure)
                 (const CHIDDeviceInputReport *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceOpen_Closure)
                 (const CHIDMessageToRemote__DeviceOpen *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceClose_Closure)
                 (const CHIDMessageToRemote__DeviceClose *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceWrite_Closure)
                 (const CHIDMessageToRemote__DeviceWrite *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceRead_Closure)
                 (const CHIDMessageToRemote__DeviceRead *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceSendFeatureReport_Closure)
                 (const CHIDMessageToRemote__DeviceSendFeatureReport *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceGetFeatureReport_Closure)
                 (const CHIDMessageToRemote__DeviceGetFeatureReport *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceGetVendorString_Closure)
                 (const CHIDMessageToRemote__DeviceGetVendorString *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceGetProductString_Closure)
                 (const CHIDMessageToRemote__DeviceGetProductString *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceGetSerialNumberString_Closure)
                 (const CHIDMessageToRemote__DeviceGetSerialNumberString *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceStartInputReports_Closure)
                 (const CHIDMessageToRemote__DeviceStartInputReports *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceRequestFullReport_Closure)
                 (const CHIDMessageToRemote__DeviceRequestFullReport *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote__DeviceDisconnect_Closure)
                 (const CHIDMessageToRemote__DeviceDisconnect *message,
                  void *closure_data);
typedef void (*CHIDMessageToRemote_Closure)
                 (const CHIDMessageToRemote *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__UpdateDeviceList_Closure)
                 (const CHIDMessageFromRemote__UpdateDeviceList *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__RequestResponse_Closure)
                 (const CHIDMessageFromRemote__RequestResponse *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport_Closure)
                 (const CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__DeviceInputReports_Closure)
                 (const CHIDMessageFromRemote__DeviceInputReports *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__CloseDevice_Closure)
                 (const CHIDMessageFromRemote__CloseDevice *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote__CloseAllDevices_Closure)
                 (const CHIDMessageFromRemote__CloseAllDevices *message,
                  void *closure_data);
typedef void (*CHIDMessageFromRemote_Closure)
                 (const CHIDMessageFromRemote *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCEnumDescriptor    ehiddevice_location__descriptor;
extern const ProtobufCEnumDescriptor    ehiddevice_disconnect_method__descriptor;
extern const ProtobufCMessageDescriptor chiddevice_info__descriptor;
extern const ProtobufCMessageDescriptor chiddevice_input_report__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_open__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_close__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_write__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_read__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_send_feature_report__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_get_feature_report__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_get_vendor_string__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_get_product_string__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_get_serial_number_string__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_start_input_reports__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_request_full_report__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_to_remote__device_disconnect__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__update_device_list__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__request_response__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__device_input_reports__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__device_input_reports__device_input_report__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__close_device__descriptor;
extern const ProtobufCMessageDescriptor chidmessage_from_remote__close_all_devices__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_protobuf_2fhiddevices_2eproto__INCLUDED */
