#ifndef XPILOT_SERVICE_NATIVE_H
#define XPILOT_SERVICE_NATIVE_H
#include "service_control.h"

/** Native service subscription, confined to its creating thread. */
typedef struct service_native service_native;
/** Create the platform backend for the fixed XPilot Infinity service.
 * @return Owned backend, or NULL on allocation failure.
 */
service_native *service_native_create(void);
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
/** Release subscriptions without stopping the service.
 * @param native Owned backend; NULL is permitted.
 */
void service_native_destroy(service_native *native);
#endif
