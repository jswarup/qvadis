# QEMU Internals Analysis & Integration Guide

## 1. QEMU Execution & Startup Flow

In standalone QEMU system emulation (`qemu-system-*`), startup is directed by `system/main.c`:

```c
int main(int argc, char **argv)
{
    qemu_init(argc, argv);
    bql_unlock();
    replay_mutex_unlock();

    if (qemu_main) {
        QemuThread main_loop_thread;
        qemu_thread_create(&main_loop_thread, "qemu_main",
                           qemu_default_main, NULL, QEMU_THREAD_DETACHED);
        return qemu_main();
    } else {
        qemu_default_main(NULL);
    }
}
```

### 1.1 Key Stages in `qemu_init(argc, argv)`
1. **Module & Type Initialization**: TypeInfo registration (`module_call_init(MODULE_INIT_QOM)`).
2. **Option Parsing**: Evaluation of command line flags (machine, CPU, accelerator, memory, drives, chardevs).
3. **Machine Class Selection**: Locates `MachineClass` (e.g. `pc-q35-11.0`) and instantiates `MachineState`.
4. **Accelerator Initialization**: Configures TCG, KVM, WHPX, or HVF.
5. **CPU Creation**: Allocates vCPUs and launches vCPU worker threads.
6. **Device & Board Setup**: Machine initialization callback `mc->init(current_machine)`.
7. **Lock State at Exit**: `qemu_init` acquires the Big QEMU Lock (BQL) and replay mutex before returning.

## 2. Event Loop Architecture

### 2.1 `aio_poll` and `qemu_main_loop_wait`
- The core event loop runs in `qemu_main_loop()`, which repeatedly invokes `main_loop_wait(false)`.
- Non-blocking pumping in embedded scenarios is achieved using:
  ```c
  main_loop_wait(nonblocking);
  ```
  or by querying the global AioContext via `qemu_get_aio_context()` and pumping with `aio_poll(ctx, blocking)`.

### 2.2 Thread Safety and BQL
- Any call interacting with device state or machine emulation must hold BQL (`bql_lock()`) and release it (`bql_unlock()`) upon completion.
- Pumping the event loop releases BQL during polling/waiting and re-acquires it before invoking pending callbacks.

## 3. Storage & Console Internals
- **BlockBackend**: High-level block device abstraction wrapping `BlockDriverState`. Drives are attached to bus controllers (VirtIO, AHCI, IDE).
- **Chardev**: Character device backend (`ChardevRingbuf`, `ChardevSocket`, or custom callbacks) enabling bi-directional streaming of serial/console I/O without physical terminals.
