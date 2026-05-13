#include <pspkernel.h>
#include <pspdebug.h>
#include "app.h"
#include "utils/log.h"

PSP_MODULE_INFO("PSP OBD2 Dashboard", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(8192);

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    app_request_exit();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("cb_thread", callback_thread,
                                     0x11, 0xFA0, PSP_THREAD_ATTR_USER, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
}

int main(void) {
    setup_callbacks();

    /* Ensure log directory exists before opening */
    sceIoMkdir("ms0:/PSP/GAME/PSP-OBD2", 0777);
    log_init("ms0:/PSP/GAME/PSP-OBD2/obd2.log");
    LOG_I("PSP OBD2 Dashboard v1.0 starting");

    if (app_init() != 0) {
        LOG_E("app_init failed, exiting");
        log_close();
        sceKernelExitGame();
        return 1;
    }

    app_run();
    app_shutdown();

    LOG_I("Clean exit");
    log_close();
    sceKernelExitGame();
    return 0;
}
