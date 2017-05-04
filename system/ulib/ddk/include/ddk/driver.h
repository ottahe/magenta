// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/types.h>
#include <magenta/listnode.h>
#include <magenta/compiler.h>
#include <stdint.h>

__BEGIN_CDECLS;

typedef struct mx_device mx_device_t;
typedef struct mx_protocol_device mx_protocol_device_t;
typedef struct mx_device_prop mx_device_prop_t;

typedef struct mx_driver mx_driver_t;
typedef struct mx_bind_inst mx_bind_inst_t;
typedef struct mx_driver_binding mx_driver_binding_t;

// echo -n "mx_driver_ops_v0.5" | sha256sum | cut -c1-16
#define DRIVER_OPS_VERSION 0x2b3490fa40d9f452

typedef struct mx_driver_ops {
    uint64_t version;   // DRIVER_OPS_VERSION

    // Opportunity to do on-load work.
    // Called ony once, before any other ops are called.
    mx_status_t (*init)(mx_driver_t* driver);

    // Requests that the driver bind to the provided device,
    // initialize it, and publish and children.
    // On success, the cookie is remembered and passed back on unbind.
    mx_status_t (*bind)(mx_driver_t* driver, mx_device_t* device, void** cookie);

    // Notifies driver that the device which the driver bound to
    // is being removed.  Called after the unbind() op of any devices
    // that are children of that device.
    void (*unbind)(mx_driver_t* driver, mx_device_t* device, void* cookie);

    // Only provided by bus manager drivers, create() is invoked to
    // instantiate a bus device instance in a new device host process
    mx_status_t (*create)(mx_driver_t* driver,
                          const char* name, const char* args,
                          mx_handle_t resource, mx_device_t** out);

    // Last call before driver is unloaded.
    mx_status_t (*release)(mx_driver_t* driver);
} mx_driver_ops_t;

struct mx_driver {
    const char* name;

    mx_driver_ops_t* ops;

    uint32_t flags;

#if !DEVHOST_V2
    struct list_node node;

    const mx_bind_inst_t* binding;
    uint32_t binding_size;
    // binding instructions
#endif
};

// echo -n "device_add_args_v0.5" | sha256sum | cut -c1-16
#define DEVICE_ADD_ARGS_VERSION 0x96a64134d56e88e3

enum {
    DEVICE_ADD_NON_BINDABLE = (1 << 0),
    DEVICE_ADD_INSTANCE     = (1 << 1),
    DEVICE_ADD_BUSDEV       = (1 << 2),
};

// Device Manager API
typedef struct {
    uint64_t version;   // DEVICE_ADD_ARGS_VERSION
    // driver name is copied to internal structure
    // max length is MX_DEVICE_NAME_MAX
    const char* name;
    // pointer to this device's driver
    mx_driver_t* driver;
    // context pointer for use by the driver
    // and passed to driver in all mx_protocol_device_t callbacks
    void* ctx;
    // pointer to device's device protocol operations
    mx_protocol_device_t* ops;
    // optional list of device properties
    mx_device_prop_t* props;
    // number of device properties
    uint32_t prop_count;
    // optional custom protocol for this device
    uint32_t proto_id;
    // optional custom protocol operations for this device
    void* proto_ops;
    // arguments used with DEVICE_ADD_BUSDEV
    const char* busdev_args;
    // resource handle used with DEVICE_ADD_BUSDEV
     mx_handle_t rsrc;
    // one or more of DEVICE_ADD_*
    uint32_t flags;
} device_add_args_t;

mx_status_t device_add(mx_device_t* parent, device_add_args_t* args, mx_device_t** out);
// Creates a device and adds it to the devmgr.
// device_add_args_t contains all "in" arguments.
// All device_add_args_t values are copied, so device_add_args_t can be stack allocated.
// The device_add_args_t.name string value is copied.
// All other pointer fields are copied as pointers.
// The newly added device will be active before this call returns, so be sure to have
// the "out" pointer point to your device-local structure so callbacks can access
// it immediately.

mx_status_t device_create(const char* name, void* ctx, mx_protocol_device_t* ops,
                          mx_driver_t* driver, mx_device_t** out);
// device_create() is used to create a device without adding it to the devmgr.
// This should only be used in mx_driver_ops_t create() callbacks for creating bus device children.

mx_status_t device_add_deprecated(mx_device_t* device, mx_device_t* parent)
  __attribute__((deprecated("use device_add instead")));
mx_status_t device_add_with_props(mx_device_t* device, mx_device_t* parent,
                                  mx_device_prop_t* props, uint32_t prop_count)
  __attribute__((deprecated("use device_add instead")));
// Adds the device to the devmgr. This makes the device visible to other drivers for binding, as
// well as through an entry in the devfs. The props must remain accessible until the device is
// unbound (or until after device_add fails).

mx_status_t device_add_instance(mx_device_t* device, mx_device_t* parent)
  __attribute__((deprecated("use device_add instead")));
mx_status_t device_add_busdev(mx_device_t* device, mx_device_t* parent,
                       mx_device_prop_t* props, uint32_t prop_count,
                       const char* args, mx_handle_t rsrc)
  __attribute__((deprecated("use device_add instead")));
mx_status_t device_remove(mx_device_t* device);
mx_status_t device_rebind(mx_device_t* device);

// Cleans up the memory allocated by device_create. Must be called during the release operation on a
// device, or if device_add fails.
void device_destroy(mx_device_t* dev)
  __attribute__((deprecated("device_destroy is no longer necessary and will be remove soon")));

// These are only for the use of core platform drivers and may return
// NULL for non-approved callers.
mx_device_t* driver_get_root_device(void);
mx_device_t* driver_get_misc_device(void);

// Devices are bindable by drivers by default.
// This can be used to prevent a device from being bound by a driver.
// Must be called before the device is added.
void device_set_bindable(mx_device_t* dev, bool bindable)
  __attribute__((deprecated("use device_add instead")));
// Sets the additional protocol for the device.
// Must be called before the device is added.
void device_set_protocol(mx_device_t* dev, uint32_t proto_id, void* proto_ops)
  __attribute__((deprecated("use device_add instead")));

void driver_unbind(mx_driver_t* driver, mx_device_t* dev);

#define ROUNDUP(a, b)   (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))
#define ALIGN(a, b) ROUNDUP(a, b)

// temporary accessor for root resource handle
mx_handle_t get_root_resource(void);

mx_status_t load_firmware(mx_driver_t* driver, const char* path,
                          mx_handle_t* fw, size_t* size);
// Drivers may need to load firmware for a device, typically during the call to
// bind the device. The devmgr will look for the firmware at the given path
// relative to system-defined locations for device firmware. The file will be
// loaded into a vmo pointed to by fw. The actual size of the firmware will be
// returned in size.

// panic is for handling non-recoverable, non-reportable fatal
// errors in a way that will get logged.  Right now this just
// does a bogus write to unmapped memory.
static inline void panic(void) {
    for (;;) {
        *((int*) 0xdead) = 1;
    }
}

// Protocol Identifiers
#define DDK_PROTOCOL_DEF(tag, val, name, flags) MX_PROTOCOL_##tag = val,
enum {
#include <ddk/protodefs.h>
};

__END_CDECLS;
