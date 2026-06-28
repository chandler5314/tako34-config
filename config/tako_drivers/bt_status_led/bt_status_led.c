#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#if IS_ENABLED(CONFIG_ZMK_USB)
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BT_STATUS_LED_NODE DT_NODELABEL(bt_status_led)

#if !DT_NODE_HAS_STATUS(BT_STATUS_LED_NODE, okay)
#error "CONFIG_TAKO_BT_STATUS_LED requires a bt_status_led devicetree node"
#endif

#define SELF_TEST_ON_MS 500
#define SELF_TEST_OFF_MS 250
#define CONNECTED_SOLID_MS 5000
#define PAIRING_BLINK_MS 500
#define RECONNECT_BLINK_MS 1000
#define UNCONNECTED_BLINK_TIMEOUT_MS 60000

enum bt_status_led_pattern {
  BT_STATUS_LED_OFF,
  BT_STATUS_LED_SELF_TEST,
  BT_STATUS_LED_CONNECTED_SOLID,
  BT_STATUS_LED_PAIRING,
  BT_STATUS_LED_RECONNECTING,
};

static const struct gpio_dt_spec bt_status_led =
    GPIO_DT_SPEC_GET(BT_STATUS_LED_NODE, gpios);
static struct k_work_delayable bt_status_led_work;
static enum bt_status_led_pattern current_pattern = BT_STATUS_LED_OFF;
static bool led_on;
static bool self_test_done;
static bool last_connected;
static uint8_t pulses_remaining;
static int64_t unconnected_blink_started_at_ms;

static void bt_status_led_set(bool on) {
  gpio_pin_set_dt(&bt_status_led, on ? 1 : 0);
  led_on = on;
}

static bool bt_status_led_usb_connected(void) {
#if IS_ENABLED(CONFIG_ZMK_USB)
  return zmk_usb_is_powered();
#else
  return false;
#endif
}

static bool bt_status_led_is_unconnected_pattern(enum bt_status_led_pattern pattern) {
  return pattern == BT_STATUS_LED_PAIRING || pattern == BT_STATUS_LED_RECONNECTING;
}

static bool bt_status_led_unconnected_timeout_expired(void) {
  if (!bt_status_led_is_unconnected_pattern(current_pattern)) {
    return false;
  }

  return k_uptime_get() - unconnected_blink_started_at_ms >= UNCONNECTED_BLINK_TIMEOUT_MS;
}

static void bt_status_led_update_state(void);

static void bt_status_led_start_pattern(enum bt_status_led_pattern pattern,
                                        uint8_t pulses) {
  k_work_cancel_delayable(&bt_status_led_work);
  current_pattern = pattern;
  pulses_remaining = pulses;
  if (bt_status_led_is_unconnected_pattern(pattern)) {
    unconnected_blink_started_at_ms = k_uptime_get();
  } else {
    unconnected_blink_started_at_ms = 0;
  }
  bt_status_led_set(false);
  k_work_schedule(&bt_status_led_work, K_NO_WAIT);
}

static void bt_status_led_stop(void) {
  k_work_cancel_delayable(&bt_status_led_work);
  current_pattern = BT_STATUS_LED_OFF;
  pulses_remaining = 0;
  unconnected_blink_started_at_ms = 0;
  bt_status_led_set(false);
}

static void bt_status_led_work_handler(struct k_work *work) {
  switch (current_pattern) {
  case BT_STATUS_LED_SELF_TEST:
    if (!led_on && pulses_remaining > 0) {
      bt_status_led_set(true);
      k_work_schedule(&bt_status_led_work, K_MSEC(SELF_TEST_ON_MS));
    } else if (led_on) {
      bt_status_led_set(false);
      pulses_remaining--;
      if (pulses_remaining == 0) {
        self_test_done = true;
        current_pattern = BT_STATUS_LED_OFF;
        bt_status_led_update_state();
      } else {
        k_work_schedule(&bt_status_led_work, K_MSEC(SELF_TEST_OFF_MS));
      }
    }
    break;

  case BT_STATUS_LED_CONNECTED_SOLID:
    if (!led_on) {
      bt_status_led_set(true);
      k_work_schedule(&bt_status_led_work, K_MSEC(CONNECTED_SOLID_MS));
    } else {
      bt_status_led_set(false);
      current_pattern = BT_STATUS_LED_OFF;
    }
    break;

  case BT_STATUS_LED_PAIRING:
    if (bt_status_led_unconnected_timeout_expired()) {
      bt_status_led_stop();
      break;
    }
    bt_status_led_set(!led_on);
    k_work_schedule(&bt_status_led_work, K_MSEC(PAIRING_BLINK_MS));
    break;

  case BT_STATUS_LED_RECONNECTING:
    if (bt_status_led_unconnected_timeout_expired()) {
      bt_status_led_stop();
      break;
    }
    bt_status_led_set(!led_on);
    k_work_schedule(&bt_status_led_work, K_MSEC(RECONNECT_BLINK_MS));
    break;

  case BT_STATUS_LED_OFF:
  default:
    bt_status_led_set(false);
    break;
  }
}

static void bt_status_led_update_state(void) {
  const bool connected = zmk_ble_active_profile_is_connected();
  const bool usb_connected = bt_status_led_usb_connected();
  const bool open_profile = zmk_ble_active_profile_is_open();

  if (!self_test_done) {
    last_connected = connected;
    return;
  }

  if (usb_connected) {
    bt_status_led_stop();
    last_connected = connected;
    return;
  }

  if (connected) {
    if (!last_connected) {
      bt_status_led_start_pattern(BT_STATUS_LED_CONNECTED_SOLID, 0);
    } else if (current_pattern != BT_STATUS_LED_CONNECTED_SOLID) {
      bt_status_led_stop();
    }
    last_connected = true;
    return;
  }

  last_connected = false;

  if (open_profile) {
    if (current_pattern != BT_STATUS_LED_PAIRING) {
      bt_status_led_start_pattern(BT_STATUS_LED_PAIRING, 0);
    }
  } else if (current_pattern != BT_STATUS_LED_RECONNECTING) {
    bt_status_led_start_pattern(BT_STATUS_LED_RECONNECTING, 0);
  }
}

static int bt_status_led_event_listener(const zmk_event_t *eh) {
  bt_status_led_update_state();
  return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(tako_bt_status_led, bt_status_led_event_listener);
ZMK_SUBSCRIPTION(tako_bt_status_led, zmk_ble_active_profile_changed);
#if IS_ENABLED(CONFIG_ZMK_USB)
ZMK_SUBSCRIPTION(tako_bt_status_led, zmk_usb_conn_state_changed);
#endif

static int bt_status_led_init(void) {
  if (!device_is_ready(bt_status_led.port)) {
    LOG_ERR("Bluetooth status LED GPIO device is not ready");
    return -ENODEV;
  }

  gpio_pin_configure_dt(&bt_status_led, GPIO_OUTPUT_INACTIVE);
  k_work_init_delayable(&bt_status_led_work, bt_status_led_work_handler);
  last_connected = zmk_ble_active_profile_is_connected();
  bt_status_led_start_pattern(BT_STATUS_LED_SELF_TEST, 2);

  return 0;
}

SYS_INIT(bt_status_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
