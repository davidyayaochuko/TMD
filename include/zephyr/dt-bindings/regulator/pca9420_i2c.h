/*
 * Copyright (c) 2021, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_REGULATOR_PCA9420_I2C_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_REGULATOR_PCA9420_I2C_H_

#include "pmic_i2c.h"

#define PCA9420_CURRENT_LIMIT_LEVELS \
	85000 0x00		/* min: 74mA, typ: 85mA, max: 98mA */\
	225000 0x20		/* min: 222mA, typ: 225mA, max: 293mA */\
	425000 0x40		/* min: 370mA, typ: 425mA, max: 489mA */\
	595000 0x60		/* min: 517mA, typ: 595mA, max: 684mA */\
	765000 0x80		/* min: 665mA, typ: 765mA, max: 880mA */\
	935000 0xA0		/* min: 813mA, typ: 935mA, max: 1075mA */\
	1105000 0xC0		/* min: 961mA, typ: 1105mA, max: 1271mA */\

#define PCA9420_MODECFG0 \
	(PMIC_MODE(0x0, 0x0, 0x0)) /* ModeCfg 0, selected via I2C */
#define PCA9420_MODECFG1 \
	(PMIC_MODE(0x4, 0x0, 0x8)) /* ModeCfg 1, selected via I2C */
#define PCA9420_MODECFG2 \
	(PMIC_MODE(0x8, 0x0, 0x10)) /* ModeCfg 2, selected via I2C */
#define PCA9420_MODECFG3 \
	(PMIC_MODE(0xC, 0x0, 0x18)) /* ModeCfg 3, selected via I2C */

/* ModeCfg 0, selected via PIN */
#define PCA9420_MODECFG0_PIN \
	(PMIC_MODE(0x0, PMIC_MODE_FLAG_MODESEL_MULTI_REG, 0x40))
/* ModeCfg 1, selected via PIN */
#define PCA9420_MODECFG1_PIN \
	(PMIC_MODE(0x4, PMIC_MODE_FLAG_MODESEL_MULTI_REG, 0x40))
/* ModeCfg 2, selected via PIN */
#define PCA9420_MODECFG2_PIN \
	(PMIC_MODE(0x8, PMIC_MODE_FLAG_MODESEL_MULTI_REG, 0x40))
/* ModeCfg 3, selected via PIN */
#define PCA9420_MODECFG3_PIN \
	(PMIC_MODE(0xC, PMIC_MODE_FLAG_MODESEL_MULTI_REG, 0x40))

/** @brief Top level system ctrl 3 */
#define PCA9420_TOP_CNTL3     0x0CU

/** @brief Mode configuration for mode 0_0 */
#define PCA9420_MODECFG_0_0          0x22U

/** @brief I2C Mode control mask */
#define PCA9420_TOP_CNTL3_MODE_I2C_MASK 0x18U

/*
 * @brief Mode control selection mask. When this bit is set, the external
 * PMIC pins MODESEL0 and MODESEL1 can be used to select the active mode
 */
#define PCA9420_MODECFG_0_MODE_CTRL_SEL_MASK 0x40U

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_REGULATOR_PCA9420_I2C_H_*/
