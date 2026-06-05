// SPDX-License-Identifier: GPL-2.0-only
/* Stubs for missing OPlus vendor functions */
#include <linux/export.h>
#include <linux/kernel.h>

int cnss_get_restart_level(void) { return 0; }
EXPORT_SYMBOL(cnss_get_restart_level);
void wl_android_wifi_bt_power_on(int on) {}
EXPORT_SYMBOL(wl_android_wifi_bt_power_on);
