/*****************************************************************************
*  Copyright 2001 - 2012 Broadcom Corporation.  All rights reserved.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <linux/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/bcmpmu59xxx.h>
#include <linux/mfd/bcmpmu59xxx_reg.h>
#include <linux/power/bcmpmu-fg.h>
#include <linux/broadcom/bcmpmu-ponkey.h>
#include <mach/rdb/brcm_rdb_include.h>
#ifdef CONFIG_CHARGER_BCMPMU_SPA
#include <linux/spa_power.h>
#endif
#if defined(CONFIG_BCMPMU_THERMAL_THROTTLE)
#include <linux/power/bcmpmu59xxx-thermal-throttle.h>
#endif
#if defined(CONFIG_BCMPMU_DIETEMP_THERMAL)
#include <linux/broadcom/bcmpmu59xxx-dietemp-thermal.h>
#endif
#if defined(CONFIG_BCMPMU_CHARGER_COOLANT)
#include <linux/bcmpmu-charger-coolant.h>
#endif

#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include "pm_params.h"
#include <plat/cpu.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define BOARD_EDN010 "Hawaiistone EDN010"
#define BOARD_EDN01x "Hawaiistone EDN01x"

#define PMU_DEVICE_I2C_ADDR	0x08
#define PMU_DEVICE_I2C_ADDR1	0x0c
#define PMU_DEVICE_INT_GPIO	29
#define PMU_DEVICE_I2C_BUSNO 4

#define PMU_DEVICE_BATTERY_SELECTION_GPIO	22

/* Maximum currents allowed to take from charger source */
#define CHRGR_CURR_SDP 500
#define CHRGR_CURR_CDP 1500
#define CHRGR_CURR_DCP 1500
#define CHRGR_CURR_TYPE1 700
#define CHRGR_CURR_TYPE2 700
#define CHRGR_CURR_PS2 100
#define CHRGR_CURR_ACA_DOCK 700
#define CHRGR_CURR_ACA 700
#define CHRGR_CURR_MISC 500

/* START Specific battery parameters for "Sony_0" */
/* Currents for COOL and WARM set to nearest upper closest HW setting.
 * It is safe since system will also consume current and the current
 * going into the battery will be slightly lesser than the targeted.
 * This is a temporary solution until dynamic system compensation
 * will be introduced.
 */
#define SONY0_BATTERY_C	390 /* C_min [mAh] */
#define SONY0_BATTERY_CURRENT_COOL 265 /* (SONY0_BATTERY_C / 2) */
#define SONY0_BATTERY_CURRENT_NORMAL ((SONY0_BATTERY_C * 80) / 39)
#define SONY0_BATTERY_CURRENT_WARM 265 /* (SONY0_BATTERY_C / 2) */

/* {min,typ,max} = {4.315, 4.345, 4.375} */
#define SONY0_BATTERY_MAX_VFLOAT_REG 0x14
#define SONY0_BATTERY_MAX_MIN_VFLOAT 4315 /* mV */
/* {min,typ,max} = {4.165 4.195 4.225} */
#define SONY0_BATTERY_WARM_VFLOAT_REG 0x0E
#define SONY0_BATTERY_WARM_VFLOAT 4165 /* mV */

/* Temperature boundaries for charging:
 * Cold <= 5
 * 5 < Cool <= 12
 * 12 < Normal < 43
 * 43 <= Warm < 55
 * Overheat >= 55
 * Temperatures in deci Celcius */
#define SONY0_BATTERY_TEMP_COOL 51
#define SONY0_BATTERY_TEMP_NORMAL 121
#define SONY0_BATTERY_TEMP_WARM 430
#define SONY0_BATTERY_TEMP_OVERHEAT 550
/* END Specific battery parameters for "Sony_0" */

/* START Specific battery parameters for "Sony_1" */
/* Currents for COOL and WARM set to nearest upper closest HW setting.
 * It is safe since system will also consume current and the current
 * going into the battery will be slightly lesser than the targeted.
 * This is a temporary solution until dynamic system compensation
 * will be introduced.
 */
#define SONY1_BATTERY_C	420 /* C_min [mAh] */
#define SONY1_BATTERY_CURRENT_COOL 265 /* ((SONY1_BATTERY_C * 6) / 10) */
#define SONY1_BATTERY_CURRENT_NORMAL (2 * SONY1_BATTERY_C)
#define SONY1_BATTERY_CURRENT_WARM 265 /* (SONY1_BATTERY_C / 2) */

/* {min,typ,max} = {4.315, 4.345, 4.375} */
#define SONY1_BATTERY_MAX_VFLOAT_REG 0x14
#define SONY1_BATTERY_MAX_MIN_VFLOAT 4315 /* mV */
/* {min,typ,max} = {4.043, 4.084, 4.125} */
#define SONY1_BATTERY_WARM_VFLOAT_REG 0x09
#define SONY1_BATTERY_WARM_VFLOAT 4043 /* mV */

/* Temperature boundaries for charging:
 * Cold <= 5
 * 5 < Cool <= 17
 * 17 < Normal < 43
 * 43 <= Warm < 55
 * Overheat >= 55
 * Temperatures in deci Celcius */
#define SONY1_BATTERY_TEMP_COOL 51
#define SONY1_BATTERY_TEMP_NORMAL 171
#define SONY1_BATTERY_TEMP_WARM 430
#define SONY1_BATTERY_TEMP_OVERHEAT 550
/* END Specific battery parameters for "Sony_1" */

/* Generic Sony battery parameters */
#define SONY_BATTERY_TEMP_HYST 30

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu);
static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu);

/* Used only when no bcmpmu dts entry found */
static struct bcmpmu59xxx_rw_data __initdata register_init_data[] = {
/* mask 0x00 is invalid value for mask */
	/* pin mux selection for pc3 and simldo1
	 * AUXONb Wakeup disabled */
	{.addr = PMU_REG_GPIOCTRL1, .val = 0x75, .mask = 0xFF},
	/*  enable PC3 function */
	{.addr = PMU_REG_GPIOCTRL2, .val = 0x0E, .mask = 0xFF},
	/* Mask Interrupt */
	{.addr = PMU_REG_INT1MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT2MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT3MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT4MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT5MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT6MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT7MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT8MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT9MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT10MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT11MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT12MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT13MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT14MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT15MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT16MSK, .val = 0xFF, .mask = 0xFF},
	/* Trickle charging timer setting */
	{.addr = PMU_REG_MBCCTRL1, .val = 0x38, .mask = 0x38},
	/*  disable software charger timer */
	{.addr = PMU_REG_MBCCTRL2, .val = 0x0, .mask = 0x04},
	/* SWUP */
	{.addr = PMU_REG_MBCCTRL3, .val = 0x04, .mask = 0x04},
	/* Enable BC12_EN */
	{.addr = PMU_REG_MBCCTRL5, .val = 0x01, .mask = 0x01},
	/* VFLOATMAX to 4.375V */
	{.addr = PMU_REG_MBCCTRL6, .val = 0x34, .mask = 0x3F},
	/*  ICCMAX to 931 +/- 5% mA */
	{.addr = PMU_REG_MBCCTRL8, .val = 0x18, .mask = 0x1F},
	/* NTC Hot/Cold enable for HW and SW charging */
	{.addr = PMU_REG_MBCCTRL11, .val = 0x0F, .mask = 0x0F},
	/* LOWBAT @ 3.40V */
	{.addr = PMU_REG_CMPCTRL3, .val = 0xC0, .mask = 0xE0},
	/* NTC Hot Temperature Comparator rising set to 55C */
	{.addr = PMU_REG_CMPCTRL5, .val = 0x4F, .mask = 0xFF},
	/* NTC Hot Temperature Comparator falling set to 52C*/
	{.addr = PMU_REG_CMPCTRL6, .val = 0x58, .mask = 0xFF},
	/* NTC Cold Temperature Comparator rising set to 5C*/
	{.addr = PMU_REG_CMPCTRL7, .val = 0xB6, .mask = 0xFF},
	/* NTC Cold Temperature Comparator falling set to 8C*/
	{.addr = PMU_REG_CMPCTRL8, .val = 0x91, .mask = 0xFF},
	/* NTC Hot/Cold Temperature Comparator bit 9,8 */
	{.addr = PMU_REG_CMPCTRL9, .val = 0x05, .mask = 0xFF},
	/* ID detection method selection
	 *  current source Trimming */
	{.addr = PMU_REG_OTGCTRL8, .val = 0xD2, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL9, .val = 0x98, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL10, .val = 0xF0, .mask = 0xFF},
	/*ADP_THR_RATIO*/
	{.addr = PMU_REG_OTGCTRL11, .val = 0x58, .mask = 0xFF},
	/* Enable ADP_PRB  ADP_DSCHG comparators */
	{.addr = PMU_REG_OTGCTRL12, .val = 0xC3, .mask = 0xFF},

/* Regulator configuration */
/* TODO regulator */
	{.addr = PMU_REG_FG_EOC_TH, .val = 0x64, .mask = 0xFF},
	{.addr = PMU_REG_RTC_C2C1_XOTRIM, .val = 0xEE, .mask = 0xFF},
	{.addr = PMU_REG_FGOCICCTRL, .val = 0x02, .mask = 0xFF},
	 /* FG power down */
	{.addr = PMU_REG_FGCTRL1, .val = 0x00, .mask = 0xFF},
	/* Enable operation mode for PC3PC2PC1 */
	{.addr = PMU_REG_GPLDO2PMCTRL2, .val = 0x00, .mask = 0xFF},
	 /* PWMLED blovk powerdown */
	{.addr =  PMU_REG_PWMLEDCTRL1, .val = 0x23, .mask = 0xFF},
	{.addr = PMU_REG_HSCP3, .val = 0x00, .mask = 0xFF},
	 /* HS audio powerdown feedback path */
	{.addr =  PMU_REG_IHF_NGMISC, .val = 0x0C, .mask = 0xFF},
	/* NTC BiasSynchronous Mode,Host Enable Control NTC_PM0 Disable*/
	{.addr =  PMU_REG_CMPCTRL14, .val = 0x13, .mask = 0xFF},
	{.addr =  PMU_REG_CMPCTRL15, .val = 0x01, .mask = 0xFF},
	/* BSI Bias Host Control, Synchronous Mode Enable */

	{.addr =  PMU_REG_CMPCTRL16, .val = 0x13, .mask = 0xFF},
	/* MBUV host enable control*/
	{.addr =  PMU_REG_CMPCTRL17, .val = 0x40, .mask = 0x7F},
	/* Mask RTM conversion */
	{.addr =  PMU_REG_ADCCTRL1, .val = 0x08, .mask = 0x08},
	/* EN_SESS_VALID  enable ID detection */
	{.addr = PMU_REG_OTGCTRL1 , .val = 0x18, .mask = 0xFF},

	/* SDSR2 NM1 voltage - 1.24 */
	{.addr = PMU_REG_SDSR2VOUT1 , .val = 0x28, .mask = 0x3F},
	/* SDSR2 LPM voltage - 1.24V */
	{.addr = PMU_REG_SDSR2VOUT2 , .val = 0x28, .mask = 0x3F},
	/* IOSR1 LPM voltage - 1.8V */
	{.addr = PMU_REG_IOSR1VOUT2 , .val = 0x3E, .mask = 0x3F},

	/* PASRCTRL MobC00256738*/
	{.addr = PMU_REG_PASRCTRL1 , .val = 0x00, .mask = 0x06},
	{.addr = PMU_REG_PASRCTRL6 , .val = 0x00, .mask = 0xF0},
	{.addr = PMU_REG_PASRCTRL7 , .val = 0x00, .mask = 0x3F},

	/*RFLDO and AUDLDO pulldown disable MobC00290043*/
	{.addr = PMU_REG_RFLDOCTRL , .val = 0x40, .mask = 0x40},
	{.addr = PMU_REG_AUDLDOCTRL , .val = 0x40, .mask = 0x40},
};

static struct bcmpmu59xxx_rw_data register_exit_data[] = {
	{.addr = PMU_REG_GPIOCTRL1, .val = 0x5, .mask = 0xFF},
	{.addr = PMU_REG_OTG_BOOSTCTRL3, .val = 0x0, .mask = 0x10},
};

__weak struct regulator_consumer_supply rf_supply[] = {
	{.supply = "rf"},
};
static struct regulator_init_data bcm59xxx_rfldo_data = {
	.constraints = {
			.name = "rfldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,
			.always_on = 0,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(rf_supply),
	.consumer_supplies = rf_supply,
};

__weak struct regulator_consumer_supply cam1_supply[] = {
	{.supply = "cam1"},
};
static struct regulator_init_data bcm59xxx_camldo1_data = {
	.constraints = {
			.name = "camldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam1_supply),
	.consumer_supplies = cam1_supply,
};

__weak struct regulator_consumer_supply cam2_supply[] = {
	{.supply = "cam2"},
};
static struct regulator_init_data bcm59xxx_camldo2_data = {
	.constraints = {
			.name = "camldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam2_supply),
	.consumer_supplies = cam2_supply,
};

__weak struct regulator_consumer_supply sim1_supply[] = {
	{.supply = "sim_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo1_data = {
	.constraints = {
			.name = "simldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim1_supply),
	.consumer_supplies = sim1_supply,
};

__weak struct regulator_consumer_supply sim2_supply[] = {
	{.supply = "sim2_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo2_data = {
	.constraints = {
			.name = "simldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim2_supply),
	.consumer_supplies = sim2_supply,
};

__weak struct regulator_consumer_supply sd_supply[] = {
	{.supply = "sd_vcc"},
	REGULATOR_SUPPLY("vddmmc", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "dummy"},
};
static struct regulator_init_data bcm59xxx_sdldo_data = {
	.constraints = {
			.name = "sdldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,

			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sd_supply),
	.consumer_supplies = sd_supply,
};
__weak struct regulator_consumer_supply sdx_supply[] = {
	{.supply = "sdx_vcc"},
	REGULATOR_SUPPLY("vddo", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "dummy"},
	{.supply = "dummy"},
};
static struct regulator_init_data bcm59xxx_sdxldo_data = {
	.constraints = {
			.name = "sdxldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdx_supply),
	.consumer_supplies = sdx_supply,
};

__weak struct regulator_consumer_supply mmc1_supply[] = {
	{.supply = "mmc1_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo1_data = {
	.constraints = {
			.name = "mmcldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = 0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc1_supply),
	.consumer_supplies = mmc1_supply,
};

__weak struct regulator_consumer_supply mmc2_supply[] = {
	{.supply = "mmc2_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo2_data = {
	.constraints = {
			.name = "mmcldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = 0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc2_supply),
	.consumer_supplies = mmc2_supply,
};

__weak struct regulator_consumer_supply aud_supply[] = {
	{.supply = "audldo_uc"},
};
static struct regulator_init_data bcm59xxx_audldo_data = {
	.constraints = {
			.name = "audldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(aud_supply),
	.consumer_supplies = aud_supply,
};

__weak struct regulator_consumer_supply usb_supply[] = {
	{.supply = "usb_vcc"},
};
static struct regulator_init_data bcm59xxx_usbldo_data = {
	.constraints = {
			.name = "usbldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(usb_supply),
	.consumer_supplies = usb_supply,
};

__weak struct regulator_consumer_supply mic_supply[] = {
	{.supply = "micldo_uc"},
};
static struct regulator_init_data bcm59xxx_micldo_data = {
	.constraints = {
			.name = "micldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = 0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mic_supply),
	.consumer_supplies = mic_supply,
};

__weak struct regulator_consumer_supply vib_supply[] = {
	{.supply = "vibldo_uc"},
};
static struct regulator_init_data bcm59xxx_vibldo_data = {
	.constraints = {
			.name = "vibldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(vib_supply),
	.consumer_supplies = vib_supply,
};

__weak struct regulator_consumer_supply gpldo1_supply[] = {
	{.supply = "gpldo1_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo1_data = {
	.constraints = {
			.name = "gpldo1",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo1_supply),
	.consumer_supplies = gpldo1_supply,
};

__weak struct regulator_consumer_supply gpldo2_supply[] = {
	{.supply = "gpldo2_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo2_data = {
	.constraints = {
			.name = "gpldo2",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo2_supply),
	.consumer_supplies = gpldo2_supply,
};

__weak struct regulator_consumer_supply gpldo3_supply[] = {
	{.supply = "sim3_vcc"},
};
static struct regulator_init_data bcm59xxx_gpldo3_data = {
	.constraints = {
			.name = "gpldo3",
			.min_uV = 1800000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo3_supply),
	.consumer_supplies = gpldo3_supply,
};

__weak struct regulator_consumer_supply tcxldo_supply[] = {
	{.supply = "tcxldo_uc"},
};
static struct regulator_init_data bcm59xxx_tcxldo_data = {
	.constraints = {
			.name = "tcxldo",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(tcxldo_supply),
	.consumer_supplies = tcxldo_supply,
};

__weak struct regulator_consumer_supply lvldo1_supply[] = {
	{.supply = "lvldo1_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo1_data = {
	.constraints = {
			.name = "lvldo1",
			.min_uV = 1000000,
			.max_uV = 1786000,
			.valid_ops_mask = 0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo1_supply),
	.consumer_supplies = lvldo1_supply,
};

__weak struct regulator_consumer_supply lvldo2_supply[] = {
	{.supply = "lvldo2_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo2_data = {
	.constraints = {
			.name = "lvldo2",
			.min_uV = 1000000,
			.max_uV = 1786000,
			.valid_ops_mask = 0,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo2_supply),
	.consumer_supplies = lvldo2_supply,
};

__weak struct regulator_consumer_supply vsr_supply[] = {
	{.supply = "vsr_uc"},
};
static struct regulator_init_data bcm59xxx_vsr_data = {
	.constraints = {
			.name = "vsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(vsr_supply),
	.consumer_supplies = vsr_supply,
};

__weak struct regulator_consumer_supply csr_supply[] = {
	{.supply = "csr_uc"},
};

static struct regulator_init_data bcm59xxx_csr_data = {
	.constraints = {
			.name = "csrldo",
			.min_uV = 700000,
			.max_uV = 1440000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
				REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(csr_supply),
	.consumer_supplies = csr_supply,
};

__weak struct regulator_consumer_supply mmsr_supply[] = {
	{.supply = "mmsr_uc"},
};

static struct regulator_init_data bcm59xxx_mmsr_data = {
	.constraints = {
			.name = "mmsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmsr_supply),
	.consumer_supplies = mmsr_supply,
};

__weak struct regulator_consumer_supply sdsr1_supply[] = {
	{.supply = "sdsr1_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr1_data = {
	.constraints = {
			.name = "sdsr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr1_supply),
	.consumer_supplies = sdsr1_supply,
};

__weak struct regulator_consumer_supply sdsr2_supply[] = {
	{.supply = "sdsr2_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr2_data = {
	.constraints = {
			.name = "sdsr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr2_supply),
	.consumer_supplies = sdsr2_supply,
};

__weak struct regulator_consumer_supply iosr1_supply[] = {
	{.supply = "iosr1_uc"},
};

static struct regulator_init_data bcm59xxx_iosr1_data = {
	.constraints = {
			.name = "iosr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr1_supply),
	.consumer_supplies = iosr1_supply,
};


__weak struct regulator_consumer_supply iosr2_supply[] = {
	{.supply = "iosr2_uc"},
};

static struct regulator_init_data bcm59xxx_iosr2_data = {
	.constraints = {
			.name = "iosr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr2_supply),
	.consumer_supplies = iosr2_supply,
};


struct bcmpmu59xxx_regulator_init_data
	bcm59xxx_regulators[BCMPMU_REGULATOR_MAX] = {
		[BCMPMU_REGULATOR_RFLDO] = {
			.id = BCMPMU_REGULATOR_RFLDO,
			.initdata = &bcm59xxx_rfldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "rf",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO1] = {
			.id = BCMPMU_REGULATOR_CAMLDO1,
			.initdata = &bcm59xxx_camldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "cam1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO2] = {
			.id = BCMPMU_REGULATOR_CAMLDO2,
			.initdata = &bcm59xxx_camldo2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "cam2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO1] = {
			.id = BCMPMU_REGULATOR_SIMLDO1,
			.initdata = &bcm59xxx_simldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "sim1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO2] = {
			.id = BCMPMU_REGULATOR_SIMLDO2,
			.initdata = &bcm59xxx_simldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "sim2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDLDO] = {
			.id = BCMPMU_REGULATOR_SDLDO,
			.initdata = &bcm59xxx_sdldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sd",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDXLDO] = {
			.id = BCMPMU_REGULATOR_SDXLDO,
			.initdata = &bcm59xxx_sdxldo_data,
			.pc_pins_map =
				 PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdx",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO1] = {
			.id = BCMPMU_REGULATOR_MMCLDO1,
			.initdata = &bcm59xxx_mmcldo1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),
			.name = "mmc1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO2] = {
			.id = BCMPMU_REGULATOR_MMCLDO2,
			.initdata = &bcm59xxx_mmcldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),
			.name = "mmc2",
			.req_volt = 0,
		},

		[BCMPMU_REGULATOR_AUDLDO] = {
			.id = BCMPMU_REGULATOR_AUDLDO,
			.initdata = &bcm59xxx_audldo_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "aud",
			.req_volt = 0,
		},

		[BCMPMU_REGULATOR_MICLDO] = {
			.id = BCMPMU_REGULATOR_MICLDO,
			.initdata = &bcm59xxx_micldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),
			.name = "mic",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_USBLDO] = {
			.id = BCMPMU_REGULATOR_USBLDO,
			.initdata = &bcm59xxx_usbldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "usb",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VIBLDO] = {
			.id = BCMPMU_REGULATOR_VIBLDO,
			.initdata = &bcm59xxx_vibldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vib",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO1] = {
			.id = BCMPMU_REGULATOR_GPLDO1,
			.initdata = &bcm59xxx_gpldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "gp1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO2] = {
			.id = BCMPMU_REGULATOR_GPLDO2,
			.initdata = &bcm59xxx_gpldo2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "gp2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO3] = {
			.id = BCMPMU_REGULATOR_GPLDO3,
			.initdata = &bcm59xxx_gpldo3_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "gp3",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_TCXLDO] = {
			.id = BCMPMU_REGULATOR_TCXLDO,
			.initdata = &bcm59xxx_tcxldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "tcx",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_LVLDO1] = {
			.id = BCMPMU_REGULATOR_LVLDO1,
			.initdata = &bcm59xxx_lvldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "lv1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_LVLDO2] = {
			.id = BCMPMU_REGULATOR_LVLDO2,
			.initdata = &bcm59xxx_lvldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),
			.name = "lv2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VSR] = {
			.id = BCMPMU_REGULATOR_VSR,
			.initdata = &bcm59xxx_vsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CSR] = {
			.id = BCMPMU_REGULATOR_CSR,
			.initdata = &bcm59xxx_csr_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC3),
			.name = "csr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMSR] = {
			.id = BCMPMU_REGULATOR_MMSR,
			.initdata = &bcm59xxx_mmsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "mmsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR1] = {
			.id = BCMPMU_REGULATOR_SDSR1,
			.initdata = &bcm59xxx_sdsr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdsr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR2] = {
			.id = BCMPMU_REGULATOR_SDSR2,
			.initdata = &bcm59xxx_sdsr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "sdsr2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR1] = {
			.id = BCMPMU_REGULATOR_IOSR1,
			.initdata = &bcm59xxx_iosr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "iosr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR2] = {
			.id = BCMPMU_REGULATOR_IOSR2,
			.initdata = &bcm59xxx_iosr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*not used*/
			.name = "iosr2",
			.req_volt = 0,
		},

	};

/*Ponkey platform data*/
struct pkey_timer_act pkey_t3_action = {
	.flags = PKEY_SMART_RST_PWR_EN,
	.action = PKEY_ACTION_SHUTDOWN,
	.timer_dly = PKEY_ACT_DELAY_1S,
	.timer_deb = PKEY_ACT_DEB_4S,
	.ctrl_params = PKEY_SR_DLY_30MS,
};

struct bcmpmu59xxx_pkey_pdata pkey_pdata = {
	.press_deb = PKEY_DEB_100MS,
	.release_deb = PKEY_DEB_100MS,
	.wakeup_deb = PKEY_WUP_DEB_500MS,
	.t3 = &pkey_t3_action,
};

struct bcmpmu59xxx_audio_pdata audio_pdata = {
	.ihf_autoseq_dis = 0,
};

struct bcmpmu59xxx_rpc_pdata rpc_pdata = {
	.delay = 30000, /*rpc delay - 30 sec*/
	.fw_delay = 5000, /* for fw_cnt use this */
	.fw_cnt = 4,
	.poll_time = 120000, /* 40c-60c 120 sec */
	.htem_poll_time = 8000, /* > 60c 8 sec */
	.mod_tem = 400, /* 40 C*/
	.htem = 600, /* 60 C*/
};


struct bcmpmu59xxx_regulator_pdata rgltr_pdata = {
	.bcmpmu_rgltr = bcm59xxx_regulators,
	.num_rgltr = ARRAY_SIZE(bcm59xxx_regulators),
};

static int chrgr_curr_lmt[BATT_MAX][PMU_CHRGR_TYPE_MAX] = {
	[BATT_0] = {
		[PMU_CHRGR_TYPE_NONE] = 0,
		[PMU_CHRGR_TYPE_SDP] = MIN(CHRGR_CURR_SDP,
					   SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_CDP] = MIN(CHRGR_CURR_CDP,
					   SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_DCP] = MIN(CHRGR_CURR_DCP,
					   SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_TYPE1] = MIN(CHRGR_CURR_TYPE1,
					     SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_TYPE2] = MIN(CHRGR_CURR_TYPE2,
					     SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_PS2] = MIN(CHRGR_CURR_PS2,
					   SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_ACA_DOCK] = MIN(CHRGR_CURR_ACA_DOCK,
						SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_ACA] = MIN(CHRGR_CURR_ACA,
					   SONY0_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_MISC] = MIN(CHRGR_CURR_MISC,
					    SONY0_BATTERY_CURRENT_NORMAL),
	},
	[BATT_1] = {
		[PMU_CHRGR_TYPE_NONE] = 0,
		[PMU_CHRGR_TYPE_SDP] = MIN(CHRGR_CURR_SDP,
					   SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_CDP] = MIN(CHRGR_CURR_CDP,
					   SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_DCP] = MIN(CHRGR_CURR_DCP,
					   SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_TYPE1] = MIN(CHRGR_CURR_TYPE1,
					     SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_TYPE2] = MIN(CHRGR_CURR_TYPE2,
					     SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_PS2] = MIN(CHRGR_CURR_PS2,
					   SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_ACA_DOCK] = MIN(CHRGR_CURR_ACA_DOCK,
						SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_ACA] = MIN(CHRGR_CURR_ACA,
					   SONY1_BATTERY_CURRENT_NORMAL),
		[PMU_CHRGR_TYPE_MISC] = MIN(CHRGR_CURR_MISC,
					    SONY1_BATTERY_CURRENT_NORMAL),
	}
};

struct bcmpmu59xxx_accy_pdata accy_pdata = {
	.flags = ACCY_USE_PM_QOS,
	.qos_pi_id = PI_MGR_PI_ID_ARM_SUB_SYSTEM,
};

struct bcmpmu_chrgr_pdata chrgr_pdata = {
	.chrgr_curr_lmt_tbl = chrgr_curr_lmt[0],
};

static struct bcmpmu_adc_lut batt_temp_map[] = {
	{34, 800},	/* 80 C */
	{40, 750},	/* 75 C */
	{47, 700},	/* 70 C */
	{56, 650},	/* 65 C */
	{66, 600},	/* 60 C */
	{79, 550},	/* 55 C */
	{82, 540},	/* 54 C */
	{85, 530},	/* 53 C */
	{88, 520},	/* 52 C */
	{94, 500},	/* 50 C */
	{113, 450},	/* 45 C */
	{117, 440},	/* 44 C */
	{121, 430},	/* 43 C */
	{126, 420},	/* 42 C */
	{135, 400},	/* 40 C */
	{162, 350},	/* 35 C */
	{193, 300},	/* 30 C */
	{230, 250},	/* 25 C */
	{273, 200},	/* 20 C */
	{292, 180},	/* 18 C */
	{302, 170},	/* 17 C */
	{312, 160},	/* 16 C */
	{323, 150},	/* 15 C */
	{333, 140},	/* 14 C */
	{344, 130},	/* 13 C */
	{355, 120},	/* 12 C */
	{378, 100},	/* 10 C */
	{401, 80},	/* 8 C */
	{413, 70},	/* 7 C */
	{425, 60},	/* 6 C */
	{438, 50},	/* 5 C */
	{502, 0},	/* 0 C */
	{568, -50},	/* -5 C */
	{634, -100},	/* -10 C */
	{698, -150},	/* -15 C */
	{758, -200},	/* -20 C */
	{812, -250},	/* -25 C */
	{858, -300},	/* -30 C */
};

struct bcmpmu_adc_pdata adc_pdata[PMU_ADC_CHANN_MAX] = {
	[PMU_ADC_CHANN_VMBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vmbatt",
					.reg = PMU_REG_ADCCTRL3,
	},
	[PMU_ADC_CHANN_VBBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbbatt",
					.reg = PMU_REG_ADCCTRL5,
	},
	[PMU_ADC_CHANN_VBUS] = {
					.flag = 0,
					.volt_range = 14400,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbus",
					.reg = PMU_REG_ADCCTRL9,
	},
	[PMU_ADC_CHANN_IDIN] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "idin",
					.reg = PMU_REG_ADCCTRL11,
	},
	[PMU_ADC_CHANN_NTC] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "ntc",
					.reg = PMU_REG_ADCCTRL13,
	},
	[PMU_ADC_CHANN_BSI] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "bsi",
					.reg = PMU_REG_ADCCTRL15,
	},
	[PMU_ADC_CHANN_BOM] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "bom",
					.reg = PMU_REG_ADCCTRL17,
	},
	[PMU_ADC_CHANN_32KTEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "32ktemp",
					.reg = PMU_REG_ADCCTRL19,
	},
	[PMU_ADC_CHANN_PATEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "patemp",
					.reg = PMU_REG_ADCCTRL21,
	},
	[PMU_ADC_CHANN_ALS] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "als",
					.reg = PMU_REG_ADCCTRL23,
	},
	[PMU_ADC_CHANN_DIE_TEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "dietemp",
					.reg = PMU_REG_ADCCTRL25,
	},
};

int bcmpmu_acld_chargers[] = {
	PMU_CHRGR_TYPE_DCP,
	PMU_CHRGR_TYPE_SDP,
	PMU_CHRGR_TYPE_TYPE2,
};

int bcmpmu_acld_curr_lmt[PMU_CHRGR_TYPE_MAX] = {
	[PMU_CHRGR_TYPE_DCP] = CHRGR_CURR_DCP,
	[PMU_CHRGR_TYPE_SDP] = CHRGR_CURR_SDP,
	[PMU_CHRGR_TYPE_TYPE2] = CHRGR_CURR_TYPE2,
};

struct bcmpmu_acld_current_data limits[BATT_MAX] = {
	[BATT_0] = {
		.i_sat = SONY0_BATTERY_CURRENT_NORMAL,
		.i_def_dcp = SONY0_BATTERY_CURRENT_NORMAL,
		.i_max_cc = SONY0_BATTERY_CURRENT_NORMAL,
		.acld_cc_lmt = SONY0_BATTERY_CURRENT_NORMAL,
		.max_charge_c_rate_percent =
		(100 * SONY0_BATTERY_CURRENT_NORMAL) / SONY0_BATTERY_C,
	},
	[BATT_1] = {
		.i_sat = SONY1_BATTERY_CURRENT_NORMAL,
		.i_def_dcp = SONY1_BATTERY_CURRENT_NORMAL,
		.i_max_cc = SONY1_BATTERY_CURRENT_NORMAL,
		.acld_cc_lmt = SONY1_BATTERY_CURRENT_NORMAL,
		.max_charge_c_rate_percent =
		(100 * SONY1_BATTERY_CURRENT_NORMAL) / SONY1_BATTERY_C,
	}
};

struct bcmpmu_acld_pdata acld_pdata = {
	.acld_vbus_margin = 200,	/*mV*/
	.acld_vbus_thrs = 5950,
	.acld_vbat_thrs = 3500,
	.acld_currents = limits,
	.otp_cc_trim = 0x1F,
	.acld_chrgrs = bcmpmu_acld_chargers,
	.acld_chrgrs_list_size = ARRAY_SIZE(bcmpmu_acld_chargers),
	.enable_adaptive_batt_curr = true,
	.batt_curr_margin = 50, /* mA */
	.acld_max_currents = bcmpmu_acld_curr_lmt,
};

/* OCV LUT */
static struct batt_volt_cap_cpt_map sony0_volt_cap_cpt_lut[] = {
	{4331, 100, 1595},
	{4287, 95, 1324},
	{4234, 90, 1350},
	{4182, 85, 1404},
	{4132, 80, 1462},
	{4084, 75, 1560},
	{4039, 70, 1671},
	{3997, 65, 1560},
	{3952, 60, 1526},
	{3906, 55, 2064},
	{3872, 50, 2700},
	{3846, 45, 3190},
	{3824, 40, 3900},
	{3806, 35, 4129},
	{3789, 30, 3190},
	{3767, 25, 2925},
	{3743, 20, 3052},
	{3720, 15, 2340},
	{3690, 10, 7020},
	{3688, 9, 7020},
	{3686, 8, 4680},
	{3683, 7, 7020},
	{3681, 6, 4680},
	{3678, 5, 1002},
	{3664, 4, 1002},
	{3650, 3, 610 },
	{3627, 2, 438 },
	{3595, 1, 72  },
	{3400, 0, 100 },

};

static struct batt_volt_cap_cpt_map sony1_volt_cap_cpt_lut[] = {
	{4329, 100, 1382},
	{4273, 95, 1488},
	{4221, 90, 1579},
	{4172, 85, 1612},
	{4124, 80, 1842},
	{4082, 75, 1407},
	{4027, 70, 1612},
	{3979, 65, 2345},
	{3946, 60, 1759},
	{3902, 55, 2211},
	{3867, 50, 2976},
	{3841, 45, 3870},
	{3821, 40, 4300},
	{3803, 35, 5160},
	{3788, 30, 3518},
	{3766, 25, 3365},
	{3743, 20, 2345},
	{3710, 15, 4837},
	{3694, 10, 7740},
	{3692, 9, 7740},
	{3690, 8, 5160},
	{3687, 7, 3870},
	{3683, 6, 645},
	{3659, 5, 645},
	{3635, 4, 418},
	{3598, 3, 309},
	{3548, 2, 309},
	{3498, 1, 157},
	{3400, 0, 100},

};

static struct batt_eoc_curr_cap_map sony0_eoc_cap_lut[] = {
	{196, 90},
	{172, 91},
	{149, 92},
	{129, 93},
	{109, 94},
	{92, 95},
	{76, 96},
	{61, 97},
	{46, 98},
	{33, 99},
	{22, 100},
	{0, 100},
};

static struct batt_eoc_curr_cap_map sony1_eoc_cap_lut[] = {
	{239, 90},
	{215, 91},
	{191, 92},
	{167, 93},
	{144, 94},
	{121, 95},
	{99, 96},
	{78, 97},
	{58, 98},
	{39, 99},
	{22, 100},
	{0, 100},
};

/* Loaded OCV LUT table with 175 mA of discharge load
 * Cutoff = OCV - ESR(OCV, 20 degC) * 175mA
 */
static struct batt_cutoff_cap_map sony0_cutoff_cap_lut[] = {
	{3501, 2},
	{3452, 1},
	{3400, 0},
};

static struct batt_cutoff_cap_map sony1_cutoff_cap_lut[] = {
	{3478, 2},
	{3413, 1},
	{3400, 0},
};

#if defined(CONFIG_BCMPMU_THERMAL_THROTTLE)
static struct batt_temp_curr_map ys_05_temp_curr_lut[] = {
		{540, 510},
		{580, 270},
		{630,  0},
};
#endif

static struct batt_esr_temp_lut sony0_esr_temp_lut[] = {
	{
		.temp = -200,
		.reset = 0, .fct = 290, .guardband = 50,
		.esr_vl_lvl = 3767, .esr_vl_slope = -14852,
		.esr_vl_offset = 64186,
		.esr_vm_lvl = 3857, .esr_vm_slope = -7909,
		.esr_vm_offset = 38028,
		.esr_vh_lvl = 4221, .esr_vh_slope = 1575,
		.esr_vh_offset = 1453,
		.esr_vf_slope = -173, .esr_vf_offset = 8831,
	},
	{
		.temp = -100,
		.reset = 0, .fct = 827, .guardband = 50,
		.esr_vl_lvl = 3750, .esr_vl_slope = -6172,
		.esr_vl_offset = 27337,
		.esr_vm_lvl = 3879, .esr_vm_slope = -4231,
		.esr_vm_offset = 20061,
		.esr_vh_lvl = 4200, .esr_vh_slope = 1270,
		.esr_vh_offset = -1280,
		.esr_vf_slope = -383, .esr_vf_offset = 5662,
	},
	{
		.temp = 0,
		.reset = 0, .fct = 973, .guardband = 30,
		.esr_vl_lvl = 3630, .esr_vl_slope = -3702,
		.esr_vl_offset = 15609,
		.esr_vm_lvl = 3895, .esr_vm_slope = -1886,
		.esr_vm_offset = 9018,
		.esr_vh_lvl = 4029, .esr_vh_slope = 1997,
		.esr_vh_offset = -6109,
		.esr_vf_slope = -269, .esr_vf_offset = 3019,
	},
	{
		.temp = 100,
		.reset = 0, .fct = 989, .guardband = 30,
		.esr_vl_lvl = 3693, .esr_vl_slope = -2223,
		.esr_vl_offset = 9159,
		.esr_vm_lvl = 3912, .esr_vm_slope = -87,
		.esr_vm_offset = 1269,
		.esr_vh_lvl = 3975, .esr_vh_slope = 2505,
		.esr_vh_offset = -8869,
		.esr_vf_slope = -400, .esr_vf_offset = 2677,
	},
	{
		.temp = 200,
		.reset = 0, .fct = 1000, .guardband = 30,
		.esr_vl_lvl = 3700, .esr_vl_slope = -3065,
		.esr_vl_offset = 11838,
		.esr_vm_lvl = 3916, .esr_vm_slope = 222,
		.esr_vm_offset = -323,
		.esr_vh_lvl = 3985, .esr_vh_slope = 2273,
		.esr_vh_offset = -8355,
		.esr_vf_slope = -525, .esr_vf_offset = 2797,
	 },
};

static struct batt_esr_temp_lut sony1_esr_temp_lut[] = {
	{
		.temp = -200,
		.reset = 0, .fct = 715, .guardband = 50,
		.esr_vl_lvl = 3674, .esr_vl_slope = -4339,
		.esr_vl_offset = 24405,
		.esr_vm_lvl = 3755, .esr_vm_slope = -25315,
		.esr_vm_offset = 101473,
		.esr_vh_lvl = 3786, .esr_vh_slope = -89672,
		.esr_vh_offset = 343156,
		.esr_vf_slope = 357, .esr_vf_offset = 2317,
	},
	{
		.temp = -100,
		.reset = 0, .fct = 854, .guardband = 50,
		.esr_vl_lvl = 3702, .esr_vl_slope = -6020,
		.esr_vl_offset = 26914,
		.esr_vm_lvl = 3808, .esr_vm_slope = -20753,
		.esr_vm_offset = 81452,
		.esr_vh_lvl = 4266, .esr_vh_slope = 677,
		.esr_vh_offset = -154,
		.esr_vf_slope = -5056, .esr_vf_offset = 24300,
	},
	{
		.temp = 0,
		.reset = 0, .fct = 982, .guardband = 30,
		.esr_vl_lvl = 3635, .esr_vl_slope = -2123,
		.esr_vl_offset = 9886,
		.esr_vm_lvl = 3822, .esr_vm_slope = -5292,
		.esr_vm_offset = 21405,
		.esr_vh_lvl = 4122, .esr_vh_slope = 324,
		.esr_vh_offset = -53,
		.esr_vf_slope = -478, .esr_vf_offset = 3250
	},
	{
		.temp = 100,
		.reset = 0, .fct = 992, .guardband = 30,
		.esr_vl_lvl = 3479, .esr_vl_slope = -9655,
		.esr_vl_offset = 34730,
		.esr_vm_lvl = 3743, .esr_vm_slope = -1718,
		.esr_vm_offset = 7118,
		.esr_vh_lvl = 4035, .esr_vh_slope = 10,
		.esr_vh_offset = 649,
		.esr_vf_slope = -333, .esr_vf_offset = 2033,
	},
	{
		.temp = 200,
		.reset = 0, .fct = 1000, .guardband = 30,
		.esr_vl_lvl = 3496, .esr_vl_slope = -6973,
		.esr_vl_offset = 25242,
		.esr_vm_lvl = 3737, .esr_vm_slope = -1713,
		.esr_vm_offset = 6850,
		.esr_vh_lvl = 4022, .esr_vh_slope = 158,
		.esr_vh_offset = -142,
		.esr_vf_slope = -302, .esr_vf_offset = 1710,
	 },
};

static struct bcmpmu_batt_property ys_05_props[BATT_MAX] = {
	[BATT_0] =  {
		.model = "Sony_0",
		.min_volt = 3400,
		.max_volt = 4350,
		.full_cap = SONY0_BATTERY_C * 3600,
		.one_c_rate = SONY0_BATTERY_C,
		.enable_flat_ocv_soc = true,
		.flat_ocv_soc_high = 50,
		.flat_ocv_soc_low = 15,
		.volt_cap_cpt_lut = sony0_volt_cap_cpt_lut,
		.volt_cap_cpt_lut_sz = ARRAY_SIZE(sony0_volt_cap_cpt_lut),
		.esr_temp_lut = sony0_esr_temp_lut,
		.esr_temp_lut_sz = ARRAY_SIZE(sony0_esr_temp_lut),
		.eoc_cap_lut = sony0_eoc_cap_lut,
		.eoc_cap_lut_sz = ARRAY_SIZE(sony0_eoc_cap_lut),
		.cutoff_cap_lut = sony0_cutoff_cap_lut,
		.cutoff_cap_lut_sz = ARRAY_SIZE(sony0_cutoff_cap_lut),
	},
	[BATT_1] = {
		.model = "Sony_1",
		.min_volt = 3400,
		.max_volt = 4350,
		.full_cap = SONY1_BATTERY_C * 3600,
		.one_c_rate = SONY1_BATTERY_C,
		.enable_flat_ocv_soc = true,
		.flat_ocv_soc_high = 50,
		.flat_ocv_soc_low = 20,
		.volt_cap_cpt_lut = sony1_volt_cap_cpt_lut,
		.volt_cap_cpt_lut_sz = ARRAY_SIZE(sony1_volt_cap_cpt_lut),
		.esr_temp_lut = sony1_esr_temp_lut,
		.esr_temp_lut_sz = ARRAY_SIZE(sony1_esr_temp_lut),
		.eoc_cap_lut = sony1_eoc_cap_lut,
		.eoc_cap_lut_sz = ARRAY_SIZE(sony1_eoc_cap_lut),
		.cutoff_cap_lut = sony1_cutoff_cap_lut,
		.cutoff_cap_lut_sz = ARRAY_SIZE(sony1_cutoff_cap_lut),
	}
};

static struct bcmpmu_batt_cap_levels ys_05_cap_levels[BATT_MAX] = {
	[BATT_0] = {
		.critical = 5,
		.low = 20,
		.normal = 75,
		.high = 95,
	},
	[BATT_1] = {
		.critical = 5,
		.low = 20,
		.normal = 75,
		.high = 95,
	},
};

static struct bcmpmu_batt_volt_levels ys_05_volt_levels[BATT_MAX] = {
	/* Loaded OCV LUT table with .sleep_current_ua of discharge load
	 * Voltage = OCV - ESR(OCV, 20 degC) * 4.5 mA
	 */
	[BATT_0] = {
		.critical = 3675, /* Not used in bcmpmu-fg.c,
				     5% loaded OCV LUT level */
		.low = 3741, /* 20% loaded OCV LUT level */
		.normal = 3800, /* Not used in bcmpmu-fg.c */
		.high = SONY0_BATTERY_MAX_MIN_VFLOAT,
		.crit_cutoff_cnt = 3,
		.vfloat_lvl = SONY0_BATTERY_MAX_VFLOAT_REG,
		.vfloat_max = SONY0_BATTERY_MAX_VFLOAT_REG,
		.vfloat_gap = 100, /* in mV */
	},
	[BATT_1] = {
		.critical = 3656, /* Not used in bcmpmu-fg.c,
				     5% loaded OCV LUT level */
		.low = 3741, /* 20% loaded OCV LUT level */
		.normal = 3800, /* Not used in bcmpmu-fg.c */
		.high = SONY1_BATTERY_MAX_MIN_VFLOAT,
		.crit_cutoff_cnt = 3,
		.vfloat_lvl = SONY1_BATTERY_MAX_VFLOAT_REG,
		.vfloat_max = SONY1_BATTERY_MAX_VFLOAT_REG,
		.vfloat_gap = 100, /* in mV */
	},
};

static struct bcmpmu_batt_cal_data ys_05_cal_data[BATT_MAX] = {
	/* Loaded OCV LUT table with .sleep_current_ua of discharge load
	 * Voltage = OCV - ESR(OCV, 20 degC) * 4.5 mA
	 */
	[BATT_0] = {
		.volt_low = 3741, /* 20% loaded OCV LUT level */
		.cap_low = 30, /* Not used in bcmpmu-fg.c */
	},
	[BATT_1] = {
		.volt_low = 3741, /* 20% loaded OCV LUT level */
		.cap_low = 30, /* Not used in bcmpmu-fg.c */
	},
};

/* temp, vfloat_lvl, vfloat_eoc, ibat_eoc */
static struct bcmpmu_fg_vf_data sony0_vf_data[] = {
	{INT_MIN, 0, 0, USHRT_MAX},
	/* EOC@4.35V,0.05C_min */
	{SONY0_BATTERY_TEMP_NORMAL, SONY0_BATTERY_MAX_VFLOAT_REG,
	 SONY0_BATTERY_MAX_MIN_VFLOAT, SONY0_BATTERY_C / 20},
	/* EOC@4.2V,0.05C_min */
	{SONY0_BATTERY_TEMP_WARM, SONY0_BATTERY_WARM_VFLOAT_REG,
	 SONY0_BATTERY_WARM_VFLOAT, SONY0_BATTERY_C / 20},
	{SONY0_BATTERY_TEMP_OVERHEAT, 0, 0, USHRT_MAX},
};

/* temp, vfloat_lvl, vfloat_eoc, ibat_eoc */
static struct bcmpmu_fg_vf_data sony1_vf_data[] = {
	{INT_MIN, 0, 0, USHRT_MAX},
	/* EOC@4.35V,0.05C_min */
	{SONY1_BATTERY_TEMP_COOL, SONY1_BATTERY_MAX_VFLOAT_REG,
	 SONY1_BATTERY_MAX_MIN_VFLOAT, SONY1_BATTERY_C / 20},
	/* EOC@4.1V,no CV */
	{SONY1_BATTERY_TEMP_WARM, SONY1_BATTERY_WARM_VFLOAT_REG,
	 SONY1_BATTERY_WARM_VFLOAT, USHRT_MAX},
	{SONY1_BATTERY_TEMP_OVERHEAT, 0, 0, USHRT_MAX},
};

static struct bcmpmu_battery_data sony_batteries[] = {
	[BATT_0] = {
		.batt_prop = &ys_05_props[BATT_0],
		.cap_levels = &ys_05_cap_levels[BATT_0],
		.volt_levels = &ys_05_volt_levels[BATT_0],
		.cal_data = &ys_05_cal_data[BATT_0],
		.eoc_current = SONY0_BATTERY_C / 20,
		.vfd = sony0_vf_data,
		.vfd_sz = ARRAY_SIZE(sony0_vf_data),
	},
	[BATT_1] = {
		.batt_prop = &ys_05_props[BATT_1],
		.cap_levels = &ys_05_cap_levels[BATT_1],
		.volt_levels = &ys_05_volt_levels[BATT_1],
		.cal_data = &ys_05_cal_data[BATT_1],
		.eoc_current = SONY1_BATTERY_C / 20,
		.vfd = sony1_vf_data,
		.vfd_sz = ARRAY_SIZE(sony1_vf_data),
	}
};

enum battery_type get_battery_type(void)
{
	static enum battery_type type = BATT_0;
	static bool found_battery;
	int ret;

	if (found_battery)
		return type;

	ret = gpio_request_one(PMU_DEVICE_BATTERY_SELECTION_GPIO,
			GPIOF_IN,
			"battery_selection");
	if (ret < 0) {
		pr_err("%s: failed requesting GPIO %d\n",
			__func__, PMU_DEVICE_BATTERY_SELECTION_GPIO);
		goto exit;
	}

	if (gpio_get_value(PMU_DEVICE_BATTERY_SELECTION_GPIO))
		type = BATT_1;

	pr_info("Found Sony battery type %d\n", type);
	found_battery = true;

	gpio_free(PMU_DEVICE_BATTERY_SELECTION_GPIO);

exit:
	return type;
}

static struct bcmpmu_fg_pdata fg_pdata = {
	.batt_data = sony_batteries,
	.batt_data_sz = ARRAY_SIZE(sony_batteries),
	.sns_resist = 10, /* Not used in bcmpmu-fg.c */
	.sys_impedence = 33, /* Not used in bcmpmu-fg.c */
	.hw_maintenance_charging = false, /* enable HW EOC of PMU */
	.enable_selective_sleep_current = true,
	.sleep_current_ua = {
		[SLEEP_DEEP] = 1450,
		[SLEEP_DISPLAY_AMBIENT] = 4500,
	}, /* floor during sleep */
	.sleep_sample_rate = 32000,
	.fg_factor = 679,
	.poll_rate_low_batt = 20000,	/* every 20 seconds */
	.poll_rate_crit_batt = 5000,	/* every 5 Seconds */
	.ntc_high_temp = 680, /*battery too hot shdwn temp*/
	.hysteresis = SONY_BATTERY_TEMP_HYST,
	.cap_delta_thrld = 20,
	.flat_cap_delta_thrld = 20,
	.disable_full_charge_learning = false,
	.full_cap_qf_sample = true,
	.saved_fc_samples = 2,
};

#if defined(CONFIG_LEDS_BCM_PMU59xxx)
static struct bcmpmu59xxx_led_pdata led_pdata = {
	.led_name = "red",
};
#endif

#if defined(CONFIG_BCMPMU_THERMAL_THROTTLE)
/* List down the Charger Registers that need to be backedup before
  *  the throttling algo starts, those registers will be restored once the
  * algo is finished.
  */
static struct chrgr_def_trim_reg_data chrgr_def_trim_reg_lut[] = {
	{.addr = PMU_REG_MBCCTRL18, .val = 0x00},
	{.addr = PMU_REG_MBCCTRL19, .val = 0x03},
	{.addr = PMU_REG_MBCCTRL20, .val = 0x02},
};

static struct bcmpmu_throttle_pdata throttle_pdata = {
	.temp_curr_lut = ys_05_temp_curr_lut,
	.temp_curr_lut_sz = ARRAY_SIZE(ys_05_temp_curr_lut),
	/* ADC channel and mode selection */
	.temp_adc_channel = PMU_ADC_CHANN_DIE_TEMP,
	.temp_adc_req_mode = PMU_ADC_REQ_SAR_MODE,
	/* Registers to store/restore while throttling*/
	.chrgr_trim_reg_lut = chrgr_def_trim_reg_lut,
	.chrgr_trim_reg_lut_sz = ARRAY_SIZE(chrgr_def_trim_reg_lut),
	.throttle_poll_time = THROTTLE_WORK_POLL_TIME,
	.hysteresis_temp = HYSTERESIS_DEFAULT_TEMP,
};
#endif

#ifdef CONFIG_BCMPMU_DIETEMP_THERMAL
struct bcmpmu_dietemp_trip dietemp_trip_points_batt0[] = {
	{.temp = 0, .type = THERMAL_TRIP_ACTIVE, .max_curr = 0,},
	{.temp = SONY0_BATTERY_TEMP_COOL, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY0_BATTERY_CURRENT_COOL,},
	{.temp = SONY0_BATTERY_TEMP_NORMAL, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY0_BATTERY_CURRENT_NORMAL,},
	{.temp = SONY0_BATTERY_TEMP_WARM, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY0_BATTERY_CURRENT_WARM,},
	{.temp = SONY0_BATTERY_TEMP_OVERHEAT, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = 0,},
};

struct bcmpmu_dietemp_trip dietemp_trip_points_batt1[] = {
	{.temp = 0, .type = THERMAL_TRIP_ACTIVE, .max_curr = 0,},
	{.temp = SONY1_BATTERY_TEMP_COOL, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY1_BATTERY_CURRENT_COOL,},
	{.temp = SONY1_BATTERY_TEMP_NORMAL, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY1_BATTERY_CURRENT_NORMAL,},
	{.temp = SONY1_BATTERY_TEMP_WARM, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = SONY1_BATTERY_CURRENT_WARM,},
	{.temp = SONY1_BATTERY_TEMP_OVERHEAT, .type = THERMAL_TRIP_ACTIVE,
	 .max_curr = 0,},
};

struct bcmpmu_dietemp_temp_zones dietemp_zones[BATT_MAX] = {
	[BATT_0] = {
		.trips = dietemp_trip_points_batt0,
		.trip_cnt = ARRAY_SIZE(dietemp_trip_points_batt0),
	},
	[BATT_1] = {
		.trips = dietemp_trip_points_batt1,
		.trip_cnt = ARRAY_SIZE(dietemp_trip_points_batt1),
	}
};

struct bcmpmu_dietemp_pdata dtemp_zone_pdata = {
	.poll_rate_ms = 5000,
	.hysteresis = SONY_BATTERY_TEMP_HYST,
	/* ADC channel and mode selection */
	.temp_adc_channel = PMU_ADC_CHANN_NTC,
	.temp_adc_req_mode = PMU_ADC_REQ_RTM_MODE,
	.dtzones = dietemp_zones,
};
#endif

#ifdef CONFIG_BCMPMU_CHARGER_COOLANT
static struct chrgr_trim_reg_data chrgr_trim_reg_lut[] = {
	{.addr = PMU_REG_MBCCTRL18, .def_val = 0x00},
	{.addr = PMU_REG_MBCCTRL19, .def_val = 0x03},
	{.addr = PMU_REG_MBCCTRL20, .def_val = 0x02},
};

static unsigned int charger_coolant_state_batt0[] = {
	0,
	SONY0_BATTERY_CURRENT_COOL,
	SONY0_BATTERY_CURRENT_NORMAL,
	SONY0_BATTERY_CURRENT_WARM,
	0
};

static unsigned int charger_coolant_state_batt1[] = {
	0,
	SONY1_BATTERY_CURRENT_COOL,
	SONY1_BATTERY_CURRENT_NORMAL,
	SONY1_BATTERY_CURRENT_WARM,
	0
};

struct charger_coolant_states coolant_states[BATT_MAX] = {
	[BATT_0] = {
		.state_no = ARRAY_SIZE(charger_coolant_state_batt0),
		.states = charger_coolant_state_batt0,
	},
	[BATT_1] = {
		.state_no = ARRAY_SIZE(charger_coolant_state_batt1),
		.states = charger_coolant_state_batt1,
	},
};

struct bcmpmu_cc_pdata ccool_pdata = {
	.coolant_states = coolant_states,
	/* Registers to store/restore while throttling*/
	.chrgr_trim_reg_lut = chrgr_trim_reg_lut,
	.chrgr_trim_reg_lut_sz = ARRAY_SIZE(chrgr_trim_reg_lut),
	.coolant_poll_time = 10000,/* 10 seconds*/
};
#endif

#ifdef CONFIG_CHARGER_BCMPMU_SPA
struct bcmpmu59xxx_spa_pb_pdata spa_pb_pdata = {
	.chrgr_name = "bcmpmu_charger",
};

struct spa_power_data spa_data = {
	.charger_name = "bcmpmu_charger",

	.suspend_temp_hot = 500,
	.recovery_temp_hot = 450,
	.suspend_temp_cold = -60,
	.recovery_temp_cold = -10,
	.eoc_current = 85,
	.recharge_voltage = 4100, /** should be < bl_84_volt_levels.high */
	.charging_cur_usb = 500,
	.charging_cur_wall = 1500,
	.charge_timer_limit = 10,
};
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/
/* The subdevices of the bcmpmu59xxx */
static struct mfd_cell pmu59xxx_devs[] = {
	{
		.name = "bcmpmu59xxx-regulator",
		.id = -1,
		.platform_data = &rgltr_pdata,
		.pdata_size = sizeof(rgltr_pdata),
	},
	{
		.name = "bcmpmu_charger",
		.id = -1,
		.platform_data = &chrgr_pdata,
		.pdata_size = sizeof(chrgr_pdata),
	},
	{
		.name = "bcmpmu_acld",
		.id = -1,
		.platform_data = &acld_pdata,
		.pdata_size = sizeof(acld_pdata),
	},
	{
		.name = "bcmpmu59xxx-ponkey",
		.id = -1,
		.platform_data = &pkey_pdata,
		.pdata_size = sizeof(pkey_pdata),
	},
	{
		.name = "bcmpmu59xxx_rtc",
		.id = -1,
	},
	{
		.name = "bcmpmu_audio",
		.id = -1,
		.platform_data = &audio_pdata,
		.pdata_size = sizeof(audio_pdata),
	},
	{
		.name = "bcmpmu_accy",
		.id = -1,
		.platform_data = &accy_pdata,
		.pdata_size = sizeof(accy_pdata),
	},
	{
		.name = "bcmpmu_accy_detect",
		.id = -1,
	},
	{
		.name = "bcmpmu_adc",
		.id = -1,
		.platform_data = adc_pdata,
		.pdata_size = sizeof(adc_pdata),
	},
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	{
		.name = "bcmpmu_spa_pb",
		.id = -1,
		.platform_data = &spa_pb_pdata,
		.pdata_size = sizeof(spa_pb_pdata),
	},
#ifdef CONFIG_SEC_CHARGING_FEATURE
	{
		.name = "spa_power",
		.id = -1,
		.platform_data = &spa_data,
		.pdata_size = sizeof(spa_data),
	},
	{
		.name = "spa_ps",
		.id = -1,
		.platform_data = NULL,
		.pdata_size = 0,
	},
#endif /*CONFIG_SEC_CHARGING_FEATURE*/
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/
	{
		.name = "bcmpmu_otg_xceiv",
		.id = -1,
	},
	{
		.name = "bcmpmu_rpc",
		.id = -1,
		.platform_data = &rpc_pdata,
		.pdata_size = sizeof(rpc_pdata),
	},
	{
		.name = "bcmpmu_fg",
		.id = -1,
		.platform_data = &fg_pdata,
		.pdata_size = sizeof(fg_pdata),
	},
#if defined(CONFIG_LEDS_BCM_PMU59xxx)
	{
		.name = "bcmpmu59xxx-led",
		.id = -1,
		.platform_data = &led_pdata,
		.pdata_size = sizeof(led_pdata),
	},
#endif
#if defined(CONFIG_BCMPMU_THERMAL_THROTTLE)
	{
		.name = "bcmpmu_thermal_throttle",
		.id = -1,
		.platform_data = &throttle_pdata,
		.pdata_size = sizeof(throttle_pdata),
	},
#endif
#if defined(CONFIG_BCMPMU_DIETEMP_THERMAL)
	{
		.name = "bcmpmu_dietemp_thermal",
		.id = -1,
		.platform_data = &dtemp_zone_pdata,
		.pdata_size = sizeof(dtemp_zone_pdata),
	},
#endif
#if defined(CONFIG_BCMPMU_CHARGER_COOLANT)
	{
		.name = "bcmpmu_charger_coolant",
		.id = -1,
		.platform_data = &ccool_pdata,
		.pdata_size = sizeof(ccool_pdata),
	},
#endif
};

static struct i2c_board_info pmu_i2c_companion_info[] = {
	{
	I2C_BOARD_INFO("bcmpmu_map1", PMU_DEVICE_I2C_ADDR1),
	},
};

static struct bcmpmu59xxx_platform_data bcmpmu_i2c_pdata = {
#if defined(CONFIG_KONA_PMU_BSC_HS_MODE)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1MHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1MHZ), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1625KHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1625KHZ), },
#else
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_50K), },
#endif
	.init = bcmpmu_init_platform_hw,
	.exit = bcmpmu_exit_platform_hw,
	.companion = BCMPMU_DUMMY_CLIENTS,
	.i2c_companion_info = pmu_i2c_companion_info,
	.i2c_adapter_id = PMU_DEVICE_I2C_BUSNO,
	.i2c_pagesize = 256,
	.bc = BCMPMU_BC_BB_BC12,
#ifdef CONFIG_CHARGER_BCMPMU_SPA
.flags = (BCMPMU_SPA_EN | BCMPMU_ACLD_EN),
#else
.flags = BCMPMU_FG_VF_CTRL | BCMPMU_ACLD_EN,
#endif
};

static struct i2c_board_info __initdata bcmpmu_i2c_info[] = {
	{
		I2C_BOARD_INFO("bcmpmu59xxx_i2c", PMU_DEVICE_I2C_ADDR),
		.platform_data = &bcmpmu_i2c_pdata,
		.irq = gpio_to_irq(PMU_DEVICE_INT_GPIO),
	},
};

int bcmpmu_get_pmu_mfd_cell(struct mfd_cell **pmu_cell)
{
	*pmu_cell  = pmu59xxx_devs;
	return ARRAY_SIZE(pmu59xxx_devs);
}
EXPORT_SYMBOL(bcmpmu_get_pmu_mfd_cell);

void bcmpmu_set_pullup_reg(void)
{
	u32 val1, val2;

	val1 = readl(KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	val2 = readl(KONA_PMU_BSC_VA + I2C_MM_HS_PADCTL_OFFSET);
	val1 |= (1 << 20 | 1 << 22);
	val2 |= (1 << I2C_MM_HS_PADCTL_PULLUP_EN_SHIFT);
	writel(val1, KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	/*      writel(val2, KONA_PMU_BSC_VA + I2C_MM_HS_PADCTL_OFFSET); */
}

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	return 0;
}

static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	pr_info("REG: pmu_exit_platform_hw called\n");
	return 0;
}

static const struct of_device_id matches[] __initconst = {
	{ .compatible = "Broadcom,bcmpmu" },
	{ }
};

static const struct of_device_id bcmpmu_regulator_dt_ids[] __initconst = {
	{ .compatible = "Broadcom,rgltr" },
	{ },
};

static const struct of_device_id bcmpmu_adc_dt_ids[] __initconst = {
	{ .compatible = "Broadcom,adc" },
	{ },
};

static int __init bcmpmu_update_pdata(char *name,
		struct bcmpmu59xxx_platform_data *bcmpmu_i2c_pdata, int init)
{
	struct device_node *np;
	struct property *prop;
	int size, i, ret = 0;
	int max = 0;
	uint32_t *p, *p1;
	struct bcmpmu59xxx_rw_data *tbl, *data;

	np = of_find_matching_node(NULL, matches);
	if (np) {
		prop = of_find_property(np, name, &size);
		if (prop == NULL) {
			pr_info("%s pmu data not found\n",
					__func__);
			return ret;
		}
		tbl = kzalloc(size, GFP_KERNEL);
		if (tbl == NULL) {
			pr_info(
					"%s Failed  alloc bcmpmu data\n"
					, __func__);
			return -ENOMEM;
		}
		p = (uint32_t *)prop->value;
		p1 = (uint32_t *)tbl;
		for (i = 0; i < size/sizeof(p); i++)
			*p1++ = be32_to_cpu(*p++);
		data = tbl;
		max = size / sizeof(struct bcmpmu59xxx_rw_data);
		ret = 1;
		for (i = 0; i < max; i++) {
			data[i].addr = ENC_PMU_REG(FIFO_MODE,
					data[i].map, data[i].addr);
		}
		if (init) {
			bcmpmu_i2c_pdata->init_data = data;
			bcmpmu_i2c_pdata->init_max = max;
		} else {
			bcmpmu_i2c_pdata->exit_data = data;
			bcmpmu_i2c_pdata->exit_max = max;
		}
	}
	return ret;
}

int __init bcmpmu_reg_init(void)
{
	struct device_node *np;
	int updt = 0;
	struct property *prop;
	const char *model;

	updt = bcmpmu_update_pdata("initdata", &bcmpmu_i2c_pdata, 1);
	if (!updt) {
		bcmpmu_i2c_pdata.init_data =  register_init_data;
		bcmpmu_i2c_pdata.init_max = ARRAY_SIZE(register_init_data);
	}

	updt = bcmpmu_update_pdata("exitdata", &bcmpmu_i2c_pdata, 0);
	if (!updt) {
		bcmpmu_i2c_pdata.exit_data =  register_exit_data;
		bcmpmu_i2c_pdata.exit_max = ARRAY_SIZE(register_exit_data);
	}

	np = of_find_matching_node(NULL, matches);
	if (np) {
		prop = of_find_property(np, "model", NULL);
		if (prop) {
			model = prop->value;
			if (!strcmp(model, BOARD_EDN010))
				bcmpmu_i2c_pdata.board_id = EDN010;
			else
				bcmpmu_i2c_pdata.board_id = EDN01x;
		} else
			bcmpmu_i2c_pdata.board_id = EDN01x;

		pr_info("Board id from dtb %x\n",
				bcmpmu_i2c_pdata.board_id);
	}

	return 0;
}

int __init rgltr_init(void)
{
	int i, j, k, int_val;
	struct device_node *np;
	struct property *prop;
	const char *output[15], *val;
	size_t total = 0, len = 0;
	np = of_find_matching_node(NULL, bcmpmu_regulator_dt_ids);
	if (!np) {
		pr_err("device tree support for rgltr not found\n");
		return 0;
	}

	for (i = 0; i < BCMPMU_REGULATOR_MAX; i++) {
		prop = of_find_property(np,
			rgltr_pdata.bcmpmu_rgltr[i].name, NULL);
		if (prop) {
			val = prop->value;
			for (j = 0, total = 0, len = 0; total < prop->length;
				len = strlen(val) + 1, total += len,
				val += len, j++)
					output[j] = val;
			/* coverity[secure_coding] */
			sscanf(output[0], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].initdata->
				num_consumer_supplies =	int_val;
			rgltr_pdata.bcmpmu_rgltr[i].initdata->
				consumer_supplies = kzalloc(sizeof(struct
					regulator_consumer_supply)*int_val,
					GFP_KERNEL);
			for (j = 0, k = 1; j < int_val; j++, k++)
				rgltr_pdata.bcmpmu_rgltr[i].initdata->
					consumer_supplies[j].supply = output[k];

			/* coverity[secure_coding] */
			sscanf(output[k++], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].initdata->
				constraints.always_on =	int_val;

			/* coverity[secure_coding] */
			sscanf(output[k++], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].pc_pins_map = int_val;

			/* coverity[secure_coding] */
			sscanf(output[k++], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].initdata->constraints.
				initial_mode =  int_val;

			/* coverity[secure_coding] */
			sscanf(output[k++], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].initdata->constraints.
				boot_on = int_val;

			/* coverity[secure_coding] */
			sscanf(output[k++], "%d", &int_val);
			rgltr_pdata.bcmpmu_rgltr[i].req_volt = int_val;
		}
	}
/* Workaround for VDDFIX leakage during deepsleep.
   Will be fixed in Java A1 revision */
	if (is_pm_erratum(ERRATUM_VDDFIX_LEAKAGE))
		bcm59xxx_csr_data.constraints.initial_mode =
			REGULATOR_MODE_IDLE;
	return 0;
}

int __init adc_init(void)
{
	int i, j;
	struct device_node *np;
	struct property *prop;
	char *output[5], *val;
	char buf[10];
	u32 addr_offset;
	size_t total = 0, len = 0;
	np = of_find_matching_node(NULL, bcmpmu_adc_dt_ids);
	if (!np) {
		pr_err("device tree support for adc not found\n");
		return 0;
	}

	for (i = 0; i < PMU_ADC_CHANN_MAX; i++) {
		/* coverity[secure_coding] */
		sprintf(buf, "channel%d", i);
		prop = of_find_property(np, buf, NULL);
		if (prop) {
			val = prop->value;
			for (j = 0, total = 0, len = 0; total < prop->length;
				len = strlen(val) + 1, total += len,
				val += len, j++)
					output[j] = val;

			if (strlen(output[0]) == 1)
				continue;
			adc_pdata[i].name = output[0];

			/* coverity[secure_coding] */
			sscanf(output[1], "%d", &adc_pdata[i].flag);
			/* coverity[secure_coding] */
			sscanf(output[2], "%d", &adc_pdata[i].volt_range);
			/* coverity[secure_coding] */
			sscanf(output[3], "%d", &adc_pdata[i].adc_offset);
			/* coverity[secure_coding] */
			sscanf(output[4], "%x", &addr_offset);
			adc_pdata[i].reg = PMU_REG_ADCCTRL1 + addr_offset;
		}
	}
	return 0;
}

int __init board_bcm59xx_init(void)
{
	int             ret = 0;
	int             irq;
	bcmpmu_reg_init();
	rgltr_init();
	adc_init();
	bcmpmu_set_pullup_reg();
	ret = gpio_request(PMU_DEVICE_INT_GPIO, "bcmpmu59xxx-irq");
	if (ret < 0) {
		pr_err("<%s> failed at gpio_request\n", __func__);
		goto exit;
	}
	ret = gpio_direction_input(PMU_DEVICE_INT_GPIO);
	if (ret < 0) {

		pr_err("%s filed at gpio_direction_input.\n",
				__func__);
		goto exit;
	}
	irq = gpio_to_irq(PMU_DEVICE_INT_GPIO);
	bcmpmu_i2c_pdata.irq = irq;
	ret  = i2c_register_board_info(PMU_DEVICE_I2C_BUSNO,
			bcmpmu_i2c_info, ARRAY_SIZE(bcmpmu_i2c_info));
	return 0;
exit:
	return ret;
}

__init int board_pmu_init(void)
{
	return board_bcm59xx_init();
}
arch_initcall(board_pmu_init);
