# Device Model Architecture

## Table of Contents
1. [QOM (QEMU Object Model)](#qom-qemu-object-model)
2. [QDEV Framework](#qdev-framework)
3. [Device Bus Architecture](#device-bus-architecture)
4. [Device Properties](#device-properties)
5. [Device Lifecycle](#device-lifecycle)
6. [Type System](#type-system)

## QOM (QEMU Object Model)

QOM provides a unified object-oriented framework for modeling QEMU components.

### QOM Hierarchy

```
Object (root)
├── Device
│   ├── SysBusDevice
│   │   ├── Timer (PIT, APIC, etc.)
│   │   ├── Interrupt Controller
│   │   └── System Controller
│   ├── PCIDevice
│   │   ├── AHCI (SATA)
│   │   ├── NIC (Network)
│   │   ├── GPU
│   │   └── USB Host
│   ├── USBDevice
│   │   ├── USB Storage
│   │   ├── USB Tablet
│   │   └── USB Keyboard
│   └── BlockDevice
│       ├── File-backed
│       ├── NBD (Network)
│       └── Snapshot
├── Machine
│   ├── PC Machine (x86)
│   ├── ARM virt Machine
│   ├── RISC-V VirtIO
│   └── Other Architectures
├── CPU
│   ├── x86-64
│   ├── ARM
│   ├── MIPS
│   └── Other Architectures
└── Backend
    ├── ChardevBackend
    ├── BlockBackend
    └── NetBackend
```

### Object Structure

```c
typedef struct Object {
    // Object identity
    const char *class_name;
    ObjectClass *class;
    
    // Object state
    ObjectProperties properties;
    
    // Parent-child relationship
    Object *parent;
    GList *children;
    
    // Reference counting
    int ref_count;
    
    // Lifecycle hooks
    void (*destructor)(Object *obj);
    
} Object;
```

### Class Structure

```c
typedef struct ObjectClass {
    // Class identification
    const char *type;
    ObjectClass *parent_class;
    
    // Type information
    size_t instance_size;
    size_t class_size;
    
    // Virtual methods
    void (*init)(Object *obj);
    void (*finalize)(Object *obj);
    void (*dispose)(Object *obj);
    
    // Properties
    GHashTable *properties;
    
    // Interfaces
    GHashTable *interfaces;
    
} ObjectClass;
```

### Property System

```
Properties enable:
├─ Configuration at instantiation
├─ Introspection
├─ Serialization (save/restore)
└─ Runtime modification

Example Property Definition:
    DEFINE_PROP_UINT32("speed", DeviceState, speed, 100)
    DEFINE_PROP_STRING("model", DeviceState, model)
    DEFINE_PROP_BOOL("enabled", DeviceState, enabled, true)
```

## QDEV Framework

QDEV is the bus-based device management layer built on QOM.

### Device Registration

```
1. Define Device Class:
   
   static const TypeInfo my_device_info = {
       .name = "my-device",
       .parent = TYPE_DEVICE,
       .instance_size = sizeof(MyDevice),
       .class_init = my_device_class_init,
       .instance_init = my_device_init,
   };

2. Register Type:
   
   type_register_static(&my_device_info);

3. Device becomes available for instantiation
```

### Device Creation Flow

```
1. Command Line / Monitor:
   -device my-device,id=dev0,speed=100
   
   ▼
2. QDEV Parse:
   ├─ Look up type "my-device"
   └─ Extract properties

3. Object Instantiation:
   ├─ Allocate memory
   ├─ Call class constructors
   └─ Invoke instance_init

4. Property Assignment:
   ├─ Set "id" property
   ├─ Set "speed" property
   └─ Notify property change handlers

5. Device Realization:
   ├─ Allocate hardware resources
   ├─ Setup interrupts
   ├─ Initialize DMA
   └─ Connect to bus

6. Return Device Handle
```

### Device State

```c
typedef struct DeviceState {
    // Object base
    Object parent_obj;
    
    // Device naming
    char *id;
    char *canonical_path;
    
    // Bus attachment
    BusState *parent_bus;
    NamedGPIOList *gpio_out;
    NamedGPIOList *gpio_in;
    
    // VM state
    int hotplugged;
    VMChangeStateEntry *vmsd_entry;
    
    // Properties
    Property *props;
    
    // Lifecycle
    bool realized;
    bool pending_deleted_event;
    
} DeviceState;
```

## Device Bus Architecture

Buses connect devices and manage device-to-device communication.

### Bus Hierarchy

```
Machine
├── System Bus (SysBus)
│   ├── Timer/Clock devices
│   ├── Interrupt Controller
│   └── UART
├── PCI Bus
│   ├── PCI Bridges
│   ├── Network Cards (VNICs)
│   ├── Storage Controllers (AHCI, SCSI)
│   └── USB Host Controllers
├── USB Bus
│   ├── USB Hub
│   ├── USB Storage
│   ├── USB Keyboard
│   └── USB Mouse
├── I2C Bus
│   ├── EEPROM
│   ├── Temperature Sensor
│   └── Voltage Regulator
└── SPI Bus
    ├── Flash Memory
    └── Sensors
```

### Bus State

```c
typedef struct BusState {
    Object parent_obj;
    
    // Bus identity
    char *name;
    
    // Bus controller (parent device)
    DeviceState *parent;
    
    // Attached devices
    QTAILQ_HEAD(, BusChild) children;
    
    // Bus type
    const BusClass *bus_class;
    
    // Hotplug support
    bool allow_hotplug;
    
} BusState;
```

### Device Attachment

```
PCI Bus Attachment:

1. PCIDevice created
   ├─ Allocated in heap
   └─ Properties initialized

2. Device attached to PCI Bus:
   ├─ Bus assigns slot/function
   ├─ Registers device in bus's device array
   └─ Updates PCI configuration space

3. PCI BAR Assignment:
   ├─ Allocate I/O address ranges
   ├─ Setup MemoryRegions
   └─ Configure device's memory map

4. Interrupt Routing:
   ├─ Assign PCI IRQ (INTA, INTB, etc.)
   ├─ Route to interrupt controller
   └─> Enable device interrupts

5. DMA Configuration:
   ├─ Setup IOMMU if present
   └─ Configure DMA boundaries
```

### Device Hotplug

```
Hotplug Addition Flow:

1. Monitor Command: device_add driver_name

2. Device Instantiation
   ├─ Create device object
   ├─ Apply properties
   └─ Notify preparation

3. Hotplug Handler Called
   ├─ Pre-plug validation
   ├─ Resource allocation
   └─ Hardware setup

4. Device Plugged
   ├─ Added to bus
   ├─ Interrupt handler registered
   └─ DMA configured

5. Guest Notification
   ├─ PCI hot-plug event (if PCI)
   ├─ ACPI notification
   └─ Guest OS discovers device

6. Guest Driver Load
   └─> Device becomes operational

Hotplug Removal Flow:

1. Monitor Command: device_del device_id

2. Guest Notification
   ├─ ACPI hot-plug event
   └─> Guest OS ejects device

3. Guest Driver Cleanup
   └─ Close device resources

4. Hotplug Handler Called
   ├─ Validate safe removal
   ├─ Cleanup state
   └─ Free resources

5. Device Removed
   ├─ Detached from bus
   └─ Memory freed
```

## Device Properties

Properties provide a standardized way to configure devices.

### Property Types

```c
// Scalar properties
DEFINE_PROP_UINT8("reg", MyDevice, reg, default_val)
DEFINE_PROP_UINT16("speed", MyDevice, speed, default_val)
DEFINE_PROP_UINT32("irq", MyDevice, irq, default_val)
DEFINE_PROP_UINT64("base_addr", MyDevice, base_addr, default_val)

// String properties
DEFINE_PROP_STRING("name", MyDevice, name)
DEFINE_PROP_STRING("mode", MyDevice, mode)

// Boolean properties
DEFINE_PROP_BOOL("enabled", MyDevice, enabled, default_val)

// Object properties (references)
DEFINE_PROP_LINK("parent", MyDevice, parent, TYPE_PARENT)

// Array properties
DEFINE_PROP_ARRAY("configs", MyDevice, configs)
```

### Property Access

```
Command-line:
    -device driver,prop1=val1,prop2=val2

Monitor:
    QMP: {"execute": "qom-set", 
          "arguments": {"path": "/device/dev0", "property": "speed", "value": 100}}
    
    HMP: qom-set /device/dev0 speed 100

QOM Property Getter:
    object_property_get_int(dev, "speed", NULL)

QOM Property Setter:
    object_property_set_int(dev, "speed", 100, NULL)
```

## Device Lifecycle

### Initialization Sequence

```
1. Type Registration
   ├─ Define TypeInfo structure
   ├─ Register with type_register_static()
   └─ Device type becomes available

2. Device Instantiation
   ├─ object_new(type)
   ├─ Allocate instance
   ├─ Call class_init()
   └─> Initialize class-level state

3. Instance Initialization
   ├─ Call instance_init()
   ├─ Initialize device state
   ├─ Setup default properties
   └─> Register properties

4. Property Application
   ├─ Apply command-line properties
   ├─ Invoke property setters
   └─> Validate property values

5. Device Realization
   ├─ Call realize() method
   ├─ Allocate hardware resources
   ├─ Setup memory regions
   ├─ Configure interrupts
   └─> Device ready for operation
```

### Cleanup Sequence

```
1. Unrealization
   ├─ Call unrealize()
   ├─ Cleanup hardware resources
   ├─ Release memory regions
   ├─ Disable interrupts
   └─> Device halted

2. Dispose
   ├─ Call dispose()
   ├─ Release internal references
   └─> Prepare for deletion

3. Finalization
   ├─ Call finalize()
   ├─ Free device state
   ├─ Release memory
   └─> Device destroyed

4. Garbage Collection
   ├─ Unref objects
   └─> Memory reclaimed
```

## Type System

### Type Definition

```c
// Define a new device type

typedef struct MyDevice {
    DeviceState parent;         // Inherit from Device
    
    // Device-specific state
    uint32_t speed;
    uint8_t *config;
    MemoryRegion mmio;
} MyDevice;

typedef struct MyDeviceClass {
    DeviceClass parent;         // Inherit from DeviceClass
    
    // Virtual methods
    void (*my_method)(MyDevice *dev);
} MyDeviceClass;

// Type registration
static const TypeInfo my_device_info = {
    .name = "my-device",
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(MyDevice),
    .class_size = sizeof(MyDeviceClass),
    .instance_init = my_device_init,
    .class_init = my_device_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    },
};

type_register_static(&my_device_info);
```

### Type Introspection

```
Query Device Types:
    QMP: {"execute": "qom-list-types"}
    
List Device Properties:
    QMP: {"execute": "device-list-properties", "arguments": {"typename": "virtio-blk"}}
    
Get Device State:
    QMP: {"execute": "qom-get", "arguments": {"path": "/device/dev0", "property": "speed"}}
    
Trace Type Hierarchy:
    QOM: object_class_get_parent() traverses class hierarchy
```

---

*See related documents: [PCI and Bus Architecture](05-pci-bus-architecture.md), [Device Model Architecture](04-device-model.md)*
