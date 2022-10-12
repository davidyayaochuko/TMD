/**
 * @file
 * @brief Internal APIs for Bluetooth CSIP
 *
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/audio/csip.h>


#define BT_CSIP_SIRK_TYPE_ENCRYPTED             0x00
#define BT_CSIP_SIRK_TYPE_PLAIN                 0x01

#define BT_CSIP_RELEASE_VALUE                   0x01
#define BT_CSIP_LOCK_VALUE                      0x02

struct csip_pending_notifications {
	bt_addr_le_t addr;
	bool pending;
	bool active;

/* Since there's a 1-to-1 connection between bonded devices, and devices in
 * the array containing this struct, if the security manager overwrites
 * the oldest keys, we also overwrite the oldest entry
 */
#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	uint32_t age;
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
};

struct bt_csip_set_sirk {
	uint8_t type;
	uint8_t value[BT_CSIP_SET_SIRK_SIZE];
} __packed;

/* TODO: Rename to bt_csip_svc_inst */
struct bt_csip_set_member_server {
	struct bt_csip_set_sirk set_sirk;
	uint8_t set_size;
	uint8_t set_lock;
	uint8_t rank;
	struct bt_csip_set_member_cb *cb;
	struct k_work_delayable set_lock_timer;
	bt_addr_le_t lock_client_addr;
	struct bt_gatt_service *service_p;
	struct csip_pending_notifications pend_notify[CONFIG_BT_MAX_PAIRED];
#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	uint32_t age_counter;
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
};

struct bt_csip {
	bool client_instance;
	union {
#if defined(CONFIG_BT_CSIP_SET_MEMBER)
		struct bt_csip_set_member_server srv;
#endif /* CONFIG_BT_CSIP_SET_MEMBER */
	};
};

struct bt_csip_set_coordinator_csis_inst *bt_csip_set_coordinator_csis_inst_by_handle(
	struct bt_conn *conn, uint16_t start_handle);
