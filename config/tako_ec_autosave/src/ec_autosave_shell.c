/*
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT zmk_kscan_ec_matrix

struct zmk_kscan_ec_matrix_calibration_event {
    enum zmk_kscan_ec_matrix_calibration_event_type {
        CALIBRATION_EV_LOW_SAMPLING_START,
        CALIBRATION_EV_HIGH_SAMPLING_START,
        CALIBRATION_EV_POSITION_LOW_DETERMINED,
        CALIBRATION_EV_POSITION_COMPLETE,
        CALIBRATION_EV_COMPLETE,
    } type;

    union zmk_kscan_ec_matrix_calibration_event_data {
        struct {
            uint8_t strobe;
            uint8_t input;
            int16_t low_avg;
            int16_t noise;
            int16_t snr;
        } position_low_determined;
        struct {
            uint8_t strobe;
            uint8_t input;
            int16_t low_avg;
            int16_t high_avg;
            int16_t noise;
            int16_t snr;
        } position_complete;
        struct {
        } calibration_complete;
    } data;
};

typedef void (*zmk_kscan_ec_matrix_calibration_cb_t)(
    const struct zmk_kscan_ec_matrix_calibration_event *ev, const void *user_data);

int zmk_kscan_ec_matrix_calibrate(const struct device *dev,
                                  zmk_kscan_ec_matrix_calibration_cb_t callback,
                                  const void *user_data);
int zmk_kscan_ec_matrix_settings_save_calibration(const struct device *dev);

#define EC_MATRIX_DEV(n) DEVICE_DT_INST_GET(n),

static const struct device *matrix_devices[] = {DT_INST_FOREACH_STATUS_OKAY(EC_MATRIX_DEV) NULL};

static const struct shell *save_shell;
static const struct device *save_dev;

static void save_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(save_work, save_work_handler);

static const struct device *get_matrix(const char *device_label) {
    for (size_t i = 0; matrix_devices[i] != NULL; i++) {
        if (strcmp(device_label, matrix_devices[i]->name) == 0) {
            return matrix_devices[i];
        }
    }

    return NULL;
}

static void save_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    const struct device *dev = save_dev;
    const struct shell *sh = save_shell;

    if (dev == NULL) {
        return;
    }

    int ret = zmk_kscan_ec_matrix_settings_save_calibration(dev);
    if (sh == NULL) {
        return;
    }

    if (ret < 0) {
        shell_print(sh, "Calibration auto-save failed (%d)", ret);
    } else {
        shell_print(sh, "Calibration saved to flash.");
    }
}

static void calibrate_cb(const struct zmk_kscan_ec_matrix_calibration_event *ev,
                         const void *user_data) {
    const struct shell *sh = (const struct shell *)user_data;

    switch (ev->type) {
    case CALIBRATION_EV_LOW_SAMPLING_START:
        shell_prompt_change(sh, "-");
        shell_print(sh, "Low value sampling begins. Please do not press any keys");
        k_sleep(K_SECONDS(1));
        break;
    case CALIBRATION_EV_HIGH_SAMPLING_START:
        shell_prompt_change(sh, "-");
        shell_print(sh, "\nHigh value sampling begins. Please slowly press each key in sequence, "
                        "releasing once an asterisk appears");
        break;
    case CALIBRATION_EV_POSITION_LOW_DETERMINED:
        shell_fprintf(sh, SHELL_NORMAL, "*");
        break;
    case CALIBRATION_EV_POSITION_COMPLETE:
        shell_fprintf(sh, SHELL_NORMAL, "*");
        break;
    case CALIBRATION_EV_COMPLETE:
        shell_prompt_change(sh, CONFIG_SHELL_PROMPT_UART);
        shell_print(sh, "\nCalibration complete! Auto-saving to flash...");
        k_work_reschedule(&save_work, K_MSEC(500));
        break;
    }
}

static int cmd_matrix_calibration_start(const struct shell *shell, size_t argc, char **argv,
                                        void *data) {
    ARG_UNUSED(argc);
    ARG_UNUSED(data);

    const struct device *dev = get_matrix(argv[-2]);
    if (dev == NULL) {
        shell_print(shell, "Unknown EC matrix device: %s", argv[-2]);
        return -ENODEV;
    }

    save_shell = shell;
    save_dev = dev;
    k_work_cancel_delayable(&save_work);

    int ret = zmk_kscan_ec_matrix_calibrate(dev, &calibrate_cb, shell);
    if (ret < 0) {
        shell_print(shell, "Failed to start calibration (%d)", ret);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_matrix_calibration_cmds,
    SHELL_CMD(start, NULL, "Calibrate the EC matrix and save to flash automatically.",
              cmd_matrix_calibration_start),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_matrix_cmds,
    SHELL_CMD(calibration, &sub_matrix_calibration_cmds, "EC calibration utilities.", NULL),
    SHELL_SUBCMD_SET_END);

static void cmd_matrix_dev_get(size_t idx, struct shell_static_entry *entry) {
    if (idx < ARRAY_SIZE(matrix_devices) - 1) {
        entry->syntax = matrix_devices[idx]->name;
        entry->handler = NULL;
        entry->subcmd = &sub_matrix_cmds;
        entry->help = "Select EC matrix device.";
    } else {
        entry->syntax = NULL;
    }
}

SHELL_DYNAMIC_CMD_CREATE(sub_ecauto_matrix_dev, cmd_matrix_dev_get);
SHELL_CMD_REGISTER(ecauto, &sub_ecauto_matrix_dev, "EC matrix auto-save commands.", NULL);
