#ifndef XPILOT_TRAY_PLATFORM_H
#define XPILOT_TRAY_PLATFORM_H
/** Run the native desktop frontend as a normal user.
 * @param argc Argument count.
 * @param argv Arguments owned by the process.
 * @return Process exit status. Exiting never stops the server service.
 */
int tray_platform_run(int argc, char **argv);
#endif
