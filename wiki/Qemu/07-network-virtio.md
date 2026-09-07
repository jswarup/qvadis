# Network and Virtio Architecture

## Table of Contents
1. [Network Architecture Overview](#network-architecture-overview)
2. [Network Backends](#network-backends)
3. [Network Devices](#network-devices)
4. [Virtio Overview](#virtio-overview)
5. [Virtio-Net Device](#virtio-net-device)
6. [Virtio Device Implementation](#virtio-device-implementation)

## Network Architecture Overview

QEMU provides virtual network device emulation with multiple backend and frontend options.

### Network Stack

```
┌──────────────────────────────┐
│   Guest Operating System      │
│   (TCP/IP Stack)             │
└────────────┬─────────────────┘
             │
┌─────────────▼──────────────────┐
│ Virtual NIC Driver              │
│ (E1000, Virtio-Net, etc.)      │
└────────────┬─────────────────────┘
             │
┌─────────────▼──────────────────┐
│ QEMU Network Device Layer       │
│ (Packet queuing, statistics)    │
└────────────┬─────────────────────┘
             │
    ┌────────┴────────┬─────────┐
    │                 │         │
    ▼                 ▼         ▼
┌─────────┐    ┌───────────┐ ┌──────────┐
│Tap/Tun  │    │ Socket    │ │ Slirp    │
│Backend  │    │ Backend   │ │ (NAT)    │
│         │    │           │ │          │
│ (Host   │    │ (Direct   │ │(Built-in │
│  kernel)│    │  socket)  │ │ TCP/IP)  │
└────┬────┘    └─────┬─────┘ └────┬─────┘
     │              │             │
     └──────────────┼─────────────┘
                    │
         ┌──────────▼──────────┐
         │   Network Backend   │
         │  (Host networking)  │
         └─────────────────────┘
             │
    ┌────────┴────────┐
    │                 │
    ▼                 ▼
 Host NIC         Host Network
 (virtio)         (bridge, NAT)
```

## Network Backends

### TAP/TUN Backend

```
Architecture:
    TAP Device (Host Kernel)
    ├─ Virtual network interface
    ├─ Raw frames to/from userspace
    └─ Requires root privileges

Guest to Host:
    Guest NIC → QEMU TAP backend → Host TAP device → Host network

Host to Guest:
    Host network → Host TAP device → QEMU TAP backend → Guest NIC

Configuration:
    -netdev tap,id=net0,ifname=tap0,script=qemu-ifup
    -device e1000,netdev=net0
    
Setup Requirements:
    1. Create TAP device (usually by qemu-ifup script)
    2. Configure IP address
    3. Add to bridge (for multi-guest networking)
    4. Enable IP forwarding
    
Performance:
    ├─ Lowest latency (direct kernel I/O)
    ├─ Highest throughput
    ├─ No emulation overhead
    └─ Requires root/special privileges
```

### Socket Backend

```
Architecture:
    TCP/UDP Socket (Host user space)
    ├─ Direct network socket
    ├─ No privilege escalation
    ├─ Limited connectivity
    └─ Useful for testing

Types:
    TCP: -netdev socket,id=net0,listen=:1234
        (Guest A connects to port 1234)
    
    UDP: -netdev socket,id=net0,udp=host:port
        (Direct UDP forwarding)
    
    Multicast: -netdev socket,id=net0,mcast=224.0.0.1:1234

Performance:
    ├─ Software packet handling
    ├─ Moderate latency
    ├─ Good for: Testing, inter-VM communication
    └─> Not for production networking
```

### SLIRP Backend (User Mode Networking)

```
Architecture:
    Built-in TCP/IP Stack (No host kernel TAP/TUN)
    ├─ Entire TCP/IP stack in QEMU
    ├─ No special privileges needed
    ├─ No host network configuration
    └─ NAT to host network

Default Configuration:
    -net nic -net user
    
    Guest DHCP: 10.0.2.0/24
    Guest IP: 10.0.2.15
    Gateway: 10.0.2.2
    DNS: 10.0.2.3
    
How It Works:
    1. Guest sends IP packet
    2. QEMU TCP/IP stack intercepts
    3. Stack translates to host address/port
    4. Stack sends to real network
    5. Response comes back
    6. Stack translates back to guest address
    
Features:
    ├─ Port forwarding: -net user,hostfwd=tcp:127.0.0.1:5022-:22
    ├─ Virtual DHCP server
    ├─ Virtual DNS server
    └─ SAMBA/SMB support for file sharing

Performance:
    ├─ Highest latency (full stack emulation)
    ├─ Lowest throughput
    ├─ Good for: Lightweight networking, testing
    └─ Not for: Performance-critical networking
```

## Network Devices

### RTL8139 (Realtek Emulated NIC)

```
Characteristics:
    ├─ Fully emulated (no para-virtualization)
    ├─ Good guest driver availability
    ├─ Moderate performance
    ├─ Higher CPU usage
    └─ Widely compatible

Architecture:
    Guest Driver → Command Registers → QEMU Emulation → Network
    
    ├─ Transmit: Guest → TX Ring → QEMU → Network
    ├─ Receive: Network → RX Ring ← QEMU ← Guest
    └─ Interrupts via IRQ line
```

### Intel e1000 (Gigabit Ethernet)

```
Characteristics:
    ├─ Gigabit-capable
    ├─ Good driver support (most OSes)
    ├─ Better performance than RTL8139
    ├─ More accurate emulation
    └─ Enterprise guest compatibility

Variants:
    ├─ e1000: Classic model
    ├─ e1000-82544gc: Gigabit variant
    ├─ e1000-82545em: Advanced features
    └─ e1000e: Newer generation

Performance:
    ├─ Medium CPU overhead
    ├─ Good throughput
    ├─ Suitable for: General use, servers
    └─ Popular default choice
```

### Virtio-Net (Para-virtual)

```
Characteristics:
    ├─ Para-virtual (guest-aware)
    ├─ Highest performance
    ├─ Lowest CPU overhead
    ├─ Excellent Linux support
    └─ Recommended for Linux guests

Why Para-virtual?
    ├─ Guest cooperates with QEMU
    ├─ Simplified device model
    ├─ Optimized data path
    ├─ Minimal emulation overhead
    └─> ~2-3x throughput vs e1000

Support:
    ├─ Linux: Built-in
    ├─ Windows: Requires driver (VirtIO driver package)
    ├─ FreeBSD: Supported
    └─ macOS: Limited support
```

## Virtio Overview

Virtio is QEMU's para-virtualization framework, providing efficient guest-host communication.

### Virtio Architecture

```
Guest                           QEMU/Host
├─ Virtio Driver                ├─ Virtio Device
│  (Guest kernel module)        │  (Emulated device)
│                               │
├─ Virtio Ring Buffers          ├─ Memory-mapped
│  (Shared memory)              │
│                               │
└─ Guest notification           └─ Host notification
   (Hypercall/MSR)              │
                                └─ Interrupt
```

### Virtio Device Model

```c
typedef struct VirtIODevice {
    const char *name;
    uint8_t device_id;          // Device type ID
    uint32_t features;          // Supported features
    uint32_t bad_features;      // Known problematic features
    
    // Ring management
    VirtQueue *vqs;             // Virtual queues
    int nvqs;                   // Number of queues
    
    // Callbacks
    void (*get_features)(VirtIODevice *vdev, uint64_t features);
    int (*set_features)(VirtIODevice *vdev, uint64_t features);
    uint64_t (*get_config)(VirtIODevice *vdev);
    void (*set_config)(VirtIODevice *vdev, const uint8_t *config);
    
    // Status
    uint8_t status;
    
} VirtIODevice;
```

### Virtio Queue (Ring)

```
Shared Memory Structure:

┌──────────────────────────────────┐
│  Descriptor Table (ring)          │
│  [0] ─┐                           │
│  [1]  ├─ Buffer descriptors       │
│  [2]  │  (guest memory addresses) │
│  ... ─┘                           │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│  Available Ring (guest→host)      │
│  [0] ─┐                           │
│  [1]  ├─ Descriptor indices       │
│  [2]  │  (guest has filled these) │
│  ... ─┘                           │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│  Used Ring (host→guest)          │
│  [0] ─┐                           │
│  [1]  ├─ Descriptor indices       │
│  [2]  │  (host has processed)     │
│  ... ─┘                           │
└──────────────────────────────────┘
```

### Virtio Operations

```
Request Processing:

1. Guest prepares data:
   ├─ Allocate buffer in guest memory
   ├─ Fill buffer with command/data
   └─> Record buffer address in descriptor

2. Guest notifies host:
   ├─ Update available ring
   ├─ Increment available index
   └─> Notify QEMU (register write or hypercall)

3. QEMU processes request:
   ├─ Read available ring
   ├─ Get buffer address from descriptor
   ├─ Access shared memory
   ├─ Process command/data
   └─> Write result to buffer

4. QEMU notifies guest:
   ├─ Update used ring
   ├─ Record completion index
   └─> Generate interrupt

5. Guest reads response:
   ├─ Read used ring
   ├─ Access result buffer
   ├─ Process result
   └─> Ready for next request
```

## Virtio-Net Device

### Virtio-Net Features

```
Basic Features:
    ├─ Multi-queue support
    ├─ MAC address configuration
    ├─ Packet filtering
    └─ Link status reporting

Advanced Features:
    ├─ Checksum offload (TCP, UDP, IP)
    ├─ LRO (Large Receive Offload)
    ├─ TSO (TCP Segmentation Offload)
    ├─ Multiqueue RX/TX
    ├─ RSS (Receive Side Scaling)
    └─ VLAN tag offload

MAC Address Configuration:
    ├─ MAC address filtering table
    ├─ Unicast filtering
    ├─ Broadcast/Multicast filtering
    └─ Promiscuous mode
```

### Packet Flow

```
Guest TX (Guest → Host):

1. Guest driver:
   ├─ Prepare packet in buffer
   ├─ Set descriptor flags
   └─> Notify QEMU

2. QEMU handler:
   ├─ Read packet from guest memory
   ├─ Route to backend (TAP, socket, etc.)
   └─> Update used ring

3. Backend:
   └─> Send to network

Guest RX (Network → Guest):

1. Network packet arrives:
   ├─ QEMU receives packet
   ├─ Find guest RX buffer
   └─> Copy packet to buffer

2. QEMU notifies guest:
   ├─ Update used ring
   └─> Generate interrupt

3. Guest driver:
   ├─ Read packet from buffer
   ├─ Process packet
   └─> Deliver to TCP/IP stack
```

## Virtio Device Implementation

### Control Plane

```
QEMU to Guest Control Messages:

1. Device Initialization:
   ├─ Guest reads device features
   ├─ Negotiates features
   ├─ Sets guest status
   └─> Device ready

2. MAC Address Setup:
   ├─ Guest issues SET_MAC_ADDRESS
   ├─ QEMU updates device state
   └─> Device uses new MAC

3. Multiqueue Setup:
   ├─ Guest issues RECEIVE_QUEUE_SET_NUM_DESC
   ├─ QEMU allocates queue resources
   └─> Queue ready for data
```

### Device Notification

```
Guest Notifications:
    Memory-mapped register write:
    Guest writes to virtio queue notify register
    → QEMU event triggered
    → QEMU processes pending requests

Host Notifications:
    MSI-X Interrupt:
    QEMU writes to interrupt address
    → Host receives interrupt
    → Guest handler called
    → Guest processes completions
```

### Multi-Queue Support

```
Single Queue:
    TX Ring → QEMU → Network
    Network → QEMU → RX Ring
    
    Bottleneck: Single thread handling all I/O

Multi-Queue (MQ):
    TX Ring 0 ──┐
    TX Ring 1  ├─→ QEMU (multiple threads) → Network
    TX Ring 2 ──┘
    
    Network → QEMU (multiple threads) ──┬→ RX Ring 0
                                         ├→ RX Ring 1
                                         └→ RX Ring 2
    
    Benefits:
    ├─ Parallel processing
    ├─ CPU affinity
    ├─ Better multicore scalability
    └─ 2-4x throughput improvement
```

---

*See related documents: [Device Model Architecture](04-device-model.md), [Advanced Topics](10-advanced-topics.md)*
