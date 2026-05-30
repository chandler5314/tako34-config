#include <nrf.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define TAKO_REGOUT0_TARGET                                                    \
  (UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos)

static void tako_nvmc_wait_ready(void) {
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
  }
}

static int tako_regout0_3v_init(void) {
  const uint32_t current = NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk;

  if (current == TAKO_REGOUT0_TARGET) {
    return 0;
  }

  if ((current & TAKO_REGOUT0_TARGET) != TAKO_REGOUT0_TARGET) {
    LOG_ERR("REGOUT0 cannot be changed to 3.0V without erasing UICR");
    return 0;
  }

  LOG_WRN("Programming REGOUT0 to 3.0V and rebooting");
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
  tako_nvmc_wait_ready();

  NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~UICR_REGOUT0_VOUT_Msk) |
                      TAKO_REGOUT0_TARGET;
  tako_nvmc_wait_ready();

  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
  tako_nvmc_wait_ready();

  sys_reboot(SYS_REBOOT_COLD);
  return 0;
}

SYS_INIT(tako_regout0_3v_init, APPLICATION, 0);
