#ifndef XPILOT_SERVICE_NATIVE_H
#define XPILOT_SERVICE_NATIVE_H
#include "service_control.h"

/** Native service subscription, confined to its creating thread. */
typedef struct service_native service_native;
/** Create the platform backend for the fixed XPilot Infinity service.
 * @return Owned backend, or NULL on allocation failure.
 */
service_native *service_native_create(void);
/** Enable normal-user desktop authorization for future requests.
 * @param native Live backend. Windows delegates calls to the installed helper;
 * Linux already uses systemd's interactive authorization protocol.
 */
void service_native_enable_authorization(service_native *native);
/** Obtain a borrowed controller interface.
 * @param native Live backend.
 * @return Function table valid until native is destroyed.
 */
service_control service_native_control(service_native *native);
/** Dispatch native events, including Windows alertable service notifications.
 * @param native Live backend.
 * @param timeout_ms Maximum wait in milliseconds; zero only drains ready events.
 */
void service_native_dispatch(service_native *native, unsigned timeout_ms);
#ifdef _WIN32
/** Add one borrowed handle to the Windows alertable event wait.
 * @param native Live backend.
 * @param handle A waitable handle, or NULL to detach. The caller owns its lifetime.
 */
void service_native_set_wait_handle(service_native *native, void *handle);
#endif
/** Release subscriptions without stopping the service.
 * @param native Owned backend; NULL is permitted.
 */
void service_native_destroy(service_native *native);
#endif
