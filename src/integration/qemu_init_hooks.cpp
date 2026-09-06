#include "qemu_context.h"
#include <iostream>

#if defined(QEMU_INTEGRATION_ENABLED)
extern "C" {
    #include "qemu/osdep.h"
    #include "qemu-main.h"
    #include "qemu/main-loop.h"
    #include "system/system.h"
    #include "system/replay.h"
    #include "block/aio.h"
}
#endif

namespace qvadis {

// QEMU integration entry hook
int QemuRunMainLoop(void* opaque) {
#if defined(QEMU_INTEGRATION_ENABLED)
    int status;
    replay_mutex_lock();
    bql_lock();
    status = qemu_main_loop();
    qemu_cleanup(status);
    bql_unlock();
    replay_mutex_unlock();
    return status;
#else
    (void)opaque;
    return 0;
#endif
}

bool QemuContext::InitQemuCore(int argc, char** argv) {
#if defined(QEMU_INTEGRATION_ENABLED)
    qemu_init(argc, argv);
    bql_unlock();
    replay_mutex_unlock();
    return true;
#else
    (void)argc;
    (void)argv;
    return true;
#endif
}

bool QemuContext::PollQemuAio(uint32_t timeout_ms) {
#if defined(QEMU_INTEGRATION_ENABLED)
    bql_lock();
    main_loop_wait(timeout_ms == 0);
    bql_unlock();
    return true;
#else
    (void)timeout_ms;
    return true;
#endif
}

} // namespace qvadis
