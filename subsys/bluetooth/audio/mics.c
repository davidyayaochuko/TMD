/*  Bluetooth MICS
 *
 * Copyright (c) 2020 Bose Corporation
 * Copyright (c) 2020-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/check.h>

#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/audio/micp.h>
#include <zephyr/bluetooth/audio/aics.h>

#include "micp_internal.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_MICP)
#define LOG_MODULE_NAME bt_micp
#include "common/log.h"

#if defined(CONFIG_BT_MICP)

static struct bt_micp micp_inst;

static void mute_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	BT_DBG("value 0x%04x", value);
}

static ssize_t read_mute(struct bt_conn *conn,
			 const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset)
{
	BT_DBG("Mute %u", micp_inst.srv.mute);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &micp_inst.srv.mute, sizeof(micp_inst.srv.mute));
}

static ssize_t write_mute(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset,
			  uint8_t flags)
{
	const uint8_t *val = buf;

	if (offset > 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len != sizeof(micp_inst.srv.mute)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if ((conn != NULL && *val == BT_MICP_MUTE_DISABLED) ||
	    *val > BT_MICP_MUTE_DISABLED) {
		return BT_GATT_ERR(BT_MICP_ERR_VAL_OUT_OF_RANGE);
	}

	if (conn != NULL && micp_inst.srv.mute == BT_MICP_MUTE_DISABLED) {
		return BT_GATT_ERR(BT_MICP_ERR_MUTE_DISABLED);
	}

	BT_DBG("%u", *val);

	if (*val != micp_inst.srv.mute) {
		micp_inst.srv.mute = *val;

		bt_gatt_notify_uuid(NULL, BT_UUID_MICS_MUTE,
				    micp_inst.srv.service_p->attrs,
				    &micp_inst.srv.mute, sizeof(micp_inst.srv.mute));

		if (micp_inst.srv.cb != NULL && micp_inst.srv.cb->mute != NULL) {
			micp_inst.srv.cb->mute(NULL, 0, micp_inst.srv.mute);
		}
	}

	return len;
}


#define DUMMY_INCLUDE(i, _) BT_GATT_INCLUDE_SERVICE(NULL),
#define AICS_INCLUDES(cnt) LISTIFY(cnt, DUMMY_INCLUDE, ())

#define BT_MICP_SERVICE_DEFINITION \
	BT_GATT_PRIMARY_SERVICE(BT_UUID_MICS), \
	AICS_INCLUDES(CONFIG_BT_MICP_AICS_INSTANCE_COUNT) \
	BT_GATT_CHARACTERISTIC(BT_UUID_MICS_MUTE, \
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, \
		BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT, \
		read_mute, write_mute, NULL), \
	BT_GATT_CCC(mute_cfg_changed, \
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT)

#define MICS_ATTR_COUNT \
	ARRAY_SIZE(((struct bt_gatt_attr []){ BT_MICP_SERVICE_DEFINITION }))
#define MICS_INCL_COUNT (CONFIG_BT_MICP_AICS_INSTANCE_COUNT)

static struct bt_gatt_attr mics_attrs[] = { BT_MICP_SERVICE_DEFINITION };
static struct bt_gatt_service mics_svc;

static int prepare_aics_inst(struct bt_micp_register_param *param)
{
	int i;
	int j;
	int err;

	for (j = 0, i = 0; i < ARRAY_SIZE(mics_attrs); i++) {
		if (bt_uuid_cmp(mics_attrs[i].uuid, BT_UUID_GATT_INCLUDE) == 0) {
			micp_inst.srv.aics_insts[j] = bt_aics_free_instance_get();
			if (micp_inst.srv.aics_insts[j] == NULL) {
				BT_DBG("Could not get free AICS instances[%u]", j);
				return -ENOMEM;
			}

			err = bt_aics_register(micp_inst.srv.aics_insts[j],
					       &param->aics_param[j]);
			if (err != 0) {
				BT_DBG("Could not register AICS instance[%u]: %d", j, err);
				return err;
			}

			mics_attrs[i].user_data = bt_aics_svc_decl_get(micp_inst.srv.aics_insts[j]);
			j++;

			if (j == CONFIG_BT_MICP_AICS_INSTANCE_COUNT) {
				break;
			}
		}
	}

	__ASSERT(j == CONFIG_BT_MICP_AICS_INSTANCE_COUNT,
		 "Invalid AICS instance count");

	return 0;
}

/****************************** PUBLIC API ******************************/
int bt_micp_register(struct bt_micp_register_param *param,
		     struct bt_micp **micp)
{
	int err;
	static bool registered;

	if (registered) {
		*micp = &micp_inst;
		return -EALREADY;
	}

	__ASSERT(param, "MICS register parameter cannot be NULL");

	if (CONFIG_BT_MICP_AICS_INSTANCE_COUNT > 0) {
		prepare_aics_inst(param);
	}

	mics_svc = (struct bt_gatt_service)BT_GATT_SERVICE(mics_attrs);
	micp_inst.srv.service_p = &mics_svc;
	err = bt_gatt_service_register(&mics_svc);

	if (err != 0) {
		BT_ERR("MICS service register failed: %d", err);
	}

	micp_inst.srv.cb = param->cb;

	*micp = &micp_inst;
	registered = true;

	return err;
}

int bt_micp_aics_deactivate(struct bt_micp *micp, struct bt_aics *inst)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp");
		return -EINVAL;
	}

	CHECKIF(inst == NULL) {
		return -EINVAL;
	}

	if (micp->client_instance) {
		BT_DBG("Can only deactivate AICS on a server instance");
		return -EINVAL;
	}

	if (CONFIG_BT_MICP_AICS_INSTANCE_COUNT > 0) {
		return bt_aics_deactivate(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_activate(struct bt_micp *micp, struct bt_aics *inst)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp");
		return -EINVAL;
	}

	CHECKIF(inst == NULL) {
		return -EINVAL;
	}

	if (micp->client_instance) {
		BT_DBG("Can only activate AICS on a server instance");
		return -EINVAL;
	}

	if (CONFIG_BT_MICP_AICS_INSTANCE_COUNT > 0) {
		return bt_aics_activate(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_mute_disable(struct bt_micp *micp)
{
	uint8_t val = BT_MICP_MUTE_DISABLED;
	int err;

	if (micp->client_instance) {
		BT_DBG("Can only disable mute on a server instance");
		return -EINVAL;
	}

	err = write_mute(NULL, NULL, &val, sizeof(val), 0, 0);

	return err > 0 ? 0 : err;
}

#endif /* CONFIG_BT_MICP */

static bool valid_aics_inst(struct bt_micp *micp, struct bt_aics *aics)
{
	if (micp == NULL) {
		return false;
	}

	if (aics == NULL) {
		return false;
	}

	if (micp->client_instance) {
		return false;
	}

#if defined(CONFIG_BT_MICP)
	for (int i = 0; i < ARRAY_SIZE(micp_inst.srv.aics_insts); i++) {
		if (micp_inst.srv.aics_insts[i] == aics) {
			return true;
		}
	}
#endif /* CONFIG_BT_MICP */
	return false;
}

int bt_micp_included_get(struct bt_micp *micp,
			 struct bt_micp_included *included)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp pointer");
		return -EINVAL;
	}

	CHECKIF(included == NULL) {
		BT_DBG("NULL service pointer");
		return -EINVAL;
	}


	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT) && micp->client_instance) {
		return bt_micp_client_included_get(micp, included);
	}

#if defined(CONFIG_BT_MICP)
	included->aics_cnt = ARRAY_SIZE(micp_inst.srv.aics_insts);
	included->aics = micp_inst.srv.aics_insts;

	return 0;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_BT_MICP */
}

int bt_micp_unmute(struct bt_micp *micp)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp pointer");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT) && micp->client_instance) {
		return bt_micp_client_unmute(micp);
	}

#if defined(CONFIG_BT_MICP)
	uint8_t val = BT_MICP_MUTE_UNMUTED;
	int err = write_mute(NULL, NULL, &val, sizeof(val), 0, 0);

	return err > 0 ? 0 : err;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_BT_MICP */
}

int bt_micp_mute(struct bt_micp *micp)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp pointer");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT) && micp->client_instance) {
		return bt_micp_client_mute(micp);
	}

#if defined(CONFIG_BT_MICP)
	uint8_t val = BT_MICP_MUTE_MUTED;
	int err = write_mute(NULL, NULL, &val, sizeof(val), 0, 0);

	return err > 0 ? 0 : err;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_BT_MICP */
}

int bt_micp_mute_get(struct bt_micp *micp)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp pointer");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT) && micp->client_instance) {
		return bt_micp_client_mute_get(micp);
	}

#if defined(CONFIG_BT_MICP)
	if (micp_inst.srv.cb && micp_inst.srv.cb->mute) {
		micp_inst.srv.cb->mute(NULL, 0, micp_inst.srv.mute);
	}

	return 0;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_BT_MICP */
}

int bt_micp_aics_state_get(struct bt_micp *micp, struct bt_aics *inst)
{
	CHECKIF(micp == NULL) {
		BT_DBG("NULL micp pointer");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_state_get(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_state_get(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_gain_setting_get(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_gain_setting_get(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_gain_setting_get(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_type_get(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_type_get(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_type_get(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_status_get(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_status_get(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_status_get(inst);
	}

	return -EOPNOTSUPP;
}
int bt_micp_aics_unmute(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_unmute(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_unmute(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_mute(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_mute(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_mute(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_manual_gain_set(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_manual_gain_set(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_manual_gain_set(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_automatic_gain_set(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_automatic_gain_set(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_automatic_gain_set(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_gain_set(struct bt_micp *micp, struct bt_aics *inst,
			  int8_t gain)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_gain_set(inst, gain);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_gain_set(inst, gain);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_description_get(struct bt_micp *micp, struct bt_aics *inst)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_description_get(inst);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_description_get(inst);
	}

	return -EOPNOTSUPP;
}

int bt_micp_aics_description_set(struct bt_micp *micp, struct bt_aics *inst,
				 const char *description)
{
	if (IS_ENABLED(CONFIG_BT_MICP_CLIENT_AICS) &&
	    bt_micp_client_valid_aics_inst(micp, inst)) {
		return bt_aics_description_set(inst, description);
	}

	if (IS_ENABLED(CONFIG_BT_MICP_AICS) &&
	    valid_aics_inst(micp, inst)) {
		return bt_aics_description_set(inst, description);
	}

	return -EOPNOTSUPP;
}
