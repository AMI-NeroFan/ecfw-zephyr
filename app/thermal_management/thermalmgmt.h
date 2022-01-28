/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __THERMAL_MGMT_H__
#define __THERMAL_MGMT_H__

#include "fan.h"
#include "thermal_sensor.h"
#include "smc.h"

/**
 * DTT - Dynamic Tuning Technology
 * ------------------------------------------------------
 * Also, previously known as DPTF - Dynamic Platform and Thermal Framework.
 *
 * Temperature Monitoring
 * ------------------------------
 * A key component of EC support in Intel DPTF is to monitor the temperature
 * of target devices and report any temperature drifts that are of significance
 * (e.g., crossing a programmed passive trip point value) to Intel DPTF
 * framework via the platform firmware. EC typically uses the shared namespace
 * to update temperature changes to the platform firmware with frequent reading
 * of the temperature data from the device itself.
 *
 * Event Generation and Hysteresis
 * -------------------------------
 * Along with temperature monitoring and reporting, EC needs to support event
 * generation to indicate to platform when an event of significance happens.
 * Intel DPTF requires platform thermal sensors to support programmable
 * auxiliary trip points such that Intel DPTF can offload some of the
 * temperature monitoring and the accompanying polling to the EC. The thermal
 * events generated by EC typically involve some level of hysteresis to avoid
 * spurious events caused by fluctuating temperatures
 *
 *
 * +--------------------------------------------------------------------------+
 * |      Hysteresis implementation in temperature monitoring                 |
 * |                                                                          |
 * +--------------------------------------------------------------------------+
 * |  Avg Temperature                                                         |
 * |    ^                                                                     |
 * |    |                                                                     |
 * |    |                                                                     |
 * |    |                                     XXXX                            |
 * |    |                                  XX    X                            |
 * |    | Trip Point                     XX       XX          XX              |
 * |    +-------------------------------X+----------XX------XX  XX----------  |
 * |    |                             XX |           XX    XX    XX           |
 * |    |                            XX  |             XXXXX      XX          |
 * |    |                          XX    |                         X          |
 * |    |     XXXXXXX             XX     |                         XX         |
 * |    |   XX      XX           XX      |                          X         |
 * |   ----XX---------XX-------XX-----------------------------------+X-----   |
 * |    |XX Hysteresis  XX    X          |                          | XX    X |
 * |    XX               XXXXX           |                          |  XX  X  |
 * |  XX|                                |                          |    XX   |
 * | X  |                                |                          |         |
 * |    |                                |                          |         |
 * |    |                                |                          |         |
 * |    |                                |                          |         |
 * |    |                                v                          v         |
 * |    |                                +-+                        +-+       |
 * |    | +------------------------------+ +------------------------+ +----+  |
 * |    |   Trip Event                                                        |
 * |    |                                                                     |
 * |  +---------------------------------------------------------------------> |
 * |    |                                                                Time |
 * |    +                                                                     |
 * |                                                                          |
 * +--------------------------------------------------------------------------+
 *
 * The figure above illustrates when events should be generated and how
 * hysteresis can be applied when monitoring platform thermal sensors via direct
 * hardware interaction. As the figure illustrates, applying a hysteresis value
 * to the monitored temperature, results in smoothing out unnecessary spikes and
 * fluctuations in the temperature monitoring and notifies the platform firmware
 * and Intel DPTF only at meaningful instances.
 *
 */

struct dtt_threshold {
	/* Status of sensor. BIT 0 -init done(1)/failed(0), BIT4 (1)- tripped */
	uint8_t status;

	/* Low temperature for notification programmed by DTT */
	int16_t low_temp;

	/* High temperature for notification programmed by DTT */
	int16_t high_temp;

	/* Temperature hysteresis */
	int16_t temp_hyst;

};

#define DTT_THRD_STATUS_BIT_INIT		0u
#define DTT_THRD_STATUS_BIT_LOW_TRIP		4u
#define DTT_THRD_STATUS_BIT_HIGH_TRIP		5u
#define CPU_TEMP_ALERT_DELTA			3u

/* Fail safe threshold */
#define THERM_SHTDWN_THRSD			103u
/* EC tolerance range 3 deg */
#define THERM_SHTDWN_EC_TOLERANCE		3u

struct therm_sensor {
	enum adc_ch_num adc_ch;
	enum acpi_thrm_sens_idx acpi_loc;
	struct dtt_threshold thrd;
};

struct therm_bsod_override_thrsd_acpi {
	/* Temp to override during BSOD */
	uint8_t temp_bsod_override;
	/* Fan speed to override during BSOD */
	uint8_t fan_bsod_override;
	/* Flag for EC to handle BSOD */
	bool is_bsod_setting_en;
	/* Flag to store bsod temp status */
	bool is_bsod_temp_crossed;
};

/* FAN override settings */
#define FAN_OVERRIDE_ACPI		75u
#define TEMP_OVERRIDE_ACPI		85u
#define TEMP_BSOD_FAN_OFF		40u

/**
 * @brief Thermal management task.
 *
 * This routine manages:
 * - Driving the fan
 * - Reading the fan speed through Tach
 * - Reading thermal sensors over ADC
 * - Reading the CPU temperature over PECI or PECI over eSPI channel.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void thermalmgmt_thread(void *p1, void *p2, void *p3);

/**
 * @brief Host API to allow BIOS set fan control override in Non-ACPI mode.
 *
 * By default, when system is not in ACPI mode, fan is self controlled by EC
 * based on local thermal policy. This API allows BIOS to gain CPU fan control
 * when system in non acpi mode.
 *
 * @param en 1 to enable BIOS fan override, otherwise 0.
 * @param duty_cycle fan duty cycle set by bios.
 */
void host_set_bios_fan_override(bool en, uint8_t duty_cycle);

/**
 * @brief Host API to allow BIOS to update critical CPU temperature.
 *
 * @param critical temperature set by bios.
 */
void host_update_crit_temp(uint8_t crit_temp);

/**
 * @brief Host API to update fan speed.
 *
 * This is ACPI hook for host to update fan duty cycle for selected fan device.
 *
 * @param idx fan device index.
 * @param duty_cycle duty cycle.
 */
void host_update_fan_speed(uint8_t idx, uint8_t duty_cycle);

/**
 * @brief API for SMC host to update dtt temperature thresholds.
 *
 * @param acpi_sen_idx sensor index in acpi table.
 * @param thrd dtt_threshold structure values to be updated.
 */
void smc_update_dtt_threshold_limits(enum acpi_thrm_sens_idx acpi_sen_idx,
				     struct dtt_threshold thrd);

/**
 * @brief API for host to update OS BSOD and fan thresholds.
 *
 * @param temp_bsod_override_val bsod override value from host.
 * @param fan_bsod_override_val fan override value from host.
 */
void host_set_bios_bsod_override(uint8_t temp_bsod_override_val,
	uint8_t fan_bsod_override_val);

/**
 * @brief API for SMC host to start the peci delay timer.
 *
 * PECI is prone to fail during boot to S0. Hence peci is not accessed
 * for a prescribed time duration.
 */
void peci_start_delay_timer(void);

/**
 * @brief API to get hardware peripherals status to host.
 *
 * This routine gets hardware peripherals (fan and thermal sensors)
 * status.
 */
void get_hw_peripherals_status(uint8_t *hw_peripherals_sts);


/**
 * @brief API for SMC host to notify the CS mode exit.
 *
 */
void thermalmgmt_handle_cs_exit(void);
#endif	/* __THERMAL_MGMT_H__ */
