/* The existing effect mapping and frame lifetimes drive the page-owned audio
 * service. Decoding is completed asynchronously before C initialization. */
#include "xpclient.h"
#include <emscripten.h>

int audioDeviceInit(char *display) { (void)display; return 0; }
EM_JS(void, play_effect, (const char *name, int type, int volume, long frame), {
    Module.xpilotAudio?.play(UTF8ToString(name), type, volume, frame);
});
void audioDevicePlay(char *filename, int type, int volume, void **private_data)
{
    (void)private_data;
    play_effect(filename, type, volume, loops);
}
void audioDeviceEvents(void) { }
EM_JS(void, update_effects, (long frame), { Module.xpilotAudio?.update(frame); });
void audioDeviceUpdate(void) { update_effects(loops); }
void audioDeviceFree(void *private_data) { (void)private_data; }
EM_JS(void, stop_effects, (void), { Module.xpilotAudio?.stop(); });
void audioDeviceClose(void) { stop_effects(); }
