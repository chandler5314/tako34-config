#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

#define BT_FORCE_LED_NODE DT_NODELABEL(bt_force_led)

static const struct gpio_dt_spec bt_force_led =
    GPIO_DT_SPEC_GET(BT_FORCE_LED_NODE, gpios);

static int bt_led_force_init(void) {
  if (!device_is_ready(bt_force_led.port)) {
    return -ENODEV;
  }

  int err = gpio_pin_configure_dt(&bt_force_led, GPIO_OUTPUT_ACTIVE);
  if (err < 0) {
    return err;
  }

  return gpio_pin_set_dt(&bt_force_led, 1);
}

SYS_INIT(bt_led_force_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
