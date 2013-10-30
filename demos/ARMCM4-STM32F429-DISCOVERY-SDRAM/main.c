/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "test.h"

#include "chprintf.h"
#include "shell.h"
#include "stmdrivers/stm32f429i_discovery_sdram.h"
#include "stmdrivers/stm32f4xx_fmc.h"

#define IS42S16400J_SIZE             0x400000

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;
  chRegSetThreadName("blinker1");
  while (TRUE) {
    palClearPad(GPIOG, GPIOG_LED4_RED);
    chThdSleepMilliseconds(500);
    palSetPad(GPIOG, GPIOG_LED4_RED);
    chThdSleepMilliseconds(500);
  }
}

/*
 * Green LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread2, 128);
static msg_t Thread2(void *arg) {

  (void)arg;
  chRegSetThreadName("blinker2");
  while (TRUE) {
    palClearPad(GPIOG, GPIOG_LED3_GREEN);
    chThdSleepMilliseconds(250);
    palSetPad(GPIOG, GPIOG_LED3_GREEN);
    chThdSleepMilliseconds(250);
  }
}

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(2048)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] = {THD_STATE_NAMES};
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n",
            (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
            (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
            states[tp->p_state], (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(),
                           TestThread, chp);
  if (tp == NULL) {
    chprintf(chp, "out of memory\r\n");
    return;
  }
  chThdWait(tp);
}

static void cmd_reset(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: reset\r\n");
    return;
  }

  chprintf(chp, "Will reset in 200ms\r\n");
  chThdSleepMilliseconds(200);
  NVIC_SystemReset();
}

static void cmd_write(BaseSequentialStream *chp, int argc, char *argv[]) {
  uint32_t counter = 0;
  uint8_t ubWritedata_8b = 0x3C;
  uint32_t uwReadwritestatus = 0;
  TimeMeasurement tm;


  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: write\r\n");
    return;
  }

  tmObjectInit(&tm);

  tmStartMeasurement(&tm);

  /* Write data value to all SDRAM memory */
  for (counter = 0; counter < IS42S16400J_SIZE; counter++)
  {
    *(__IO uint8_t*) (SDRAM_BANK_ADDR + counter) = (uint8_t)(ubWritedata_8b + counter);
  }

  tmStopMeasurement(&tm);
  uint32_t write_ms = RTT2MS(tm.last);

  if (!uwReadwritestatus) {
    chprintf(chp, "SDRAM written in %dms.\r\n", write_ms);
  }

}

static void cmd_erase(BaseSequentialStream *chp, int argc, char *argv[]) {
  uint32_t counter = 0;
  uint32_t uwReadwritestatus = 0;
  TimeMeasurement tm;


  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: erase\r\n");
    return;
  }

  tmObjectInit(&tm);

  tmStartMeasurement(&tm);

  /* Write data value to all SDRAM memory */
  /* Erase SDRAM memory */
  for (counter = 0; counter < IS42S16400J_SIZE; counter++)
  {
    *(__IO uint8_t*) (SDRAM_BANK_ADDR + counter) = (uint8_t)0x0;
  }

  tmStopMeasurement(&tm);
  uint32_t write_ms = RTT2MS(tm.last);

  if (!uwReadwritestatus) {
    chprintf(chp, "SDRAM erased in %dms.\r\n", write_ms);
  }

}

static void cmd_selfrefresh(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;

  FMC_SDRAMCommandTypeDef FMC_SDRAMCommandStructure;

  if (argc > 0) {
    chprintf(chp, "Usage: selfrefresh\r\n");
    return;
  }

  /* Program a self-refresh mode command */
  FMC_SDRAMCommandStructure.FMC_CommandMode = FMC_Command_Mode_Selfrefresh;
  FMC_SDRAMCommandStructure.FMC_CommandTarget = FMC_Command_Target_bank2;
  FMC_SDRAMCommandStructure.FMC_AutoRefreshNumber = 1;
  FMC_SDRAMCommandStructure.FMC_ModeRegisterDefinition = 0;

  /* Send the command */
  FMC_SDRAMCmdConfig(&FMC_SDRAMCommandStructure);

  /* Wait until the SDRAM controller is ready */
  while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET) {
  }

  /* Check the bank mode status */
  if(FMC_GetModeStatus(FMC_Bank2_SDRAM) != FMC_SelfRefreshMode_Status) {
    chprintf(chp, "SDRAM is not in self refresh mode, command FAILED.\r\n");
  } else {
    chprintf(chp, "SDRAM is in self refresh mode.\r\n");
  }

}

static void cmd_normal(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;

  FMC_SDRAMCommandTypeDef FMC_SDRAMCommandStructure;

  if (argc > 0) {
    chprintf(chp, "Usage: normal\r\n");
    return;
  }

  /* Program a self-refresh mode command */
  FMC_SDRAMCommandStructure.FMC_CommandMode = FMC_Command_Mode_normal;
  FMC_SDRAMCommandStructure.FMC_CommandTarget = FMC_Command_Target_bank2;
  FMC_SDRAMCommandStructure.FMC_AutoRefreshNumber = 1;
  FMC_SDRAMCommandStructure.FMC_ModeRegisterDefinition = 0;

  /* Send the command */
  FMC_SDRAMCmdConfig(&FMC_SDRAMCommandStructure);

  /* Wait until the SDRAM controller is ready */
  while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET) {
  }

  /* Check the bank mode status */
  if(FMC_GetModeStatus(FMC_Bank2_SDRAM) != FMC_NormalMode_Status) {
    chprintf(chp, "SDRAM is not in normal mode, command FAILED.\r\n");
  } else {
    chprintf(chp, "SDRAM is in normal mode.\r\n");
  }

}

static void cmd_check(BaseSequentialStream *chp, int argc, char *argv[]) {
  uint32_t counter = 0;
  uint8_t ubWritedata_8b = 0x3C, ubReaddata_8b = 0;
  uint32_t uwReadwritestatus = 0;
  TimeMeasurement tm;


  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: check\r\n");
    return;
  }

  tmObjectInit(&tm);

  tmStartMeasurement(&tm);

  /* Read back SDRAM memory and check content correctness*/
  counter = 0;
  uwReadwritestatus = 0;
  while ((counter < IS42S16400J_SIZE) && (uwReadwritestatus == 0))
  {
    ubReaddata_8b = *(__IO uint8_t*)(SDRAM_BANK_ADDR + counter);
    if ( ubReaddata_8b != (uint8_t)(ubWritedata_8b + counter))
    {
      uwReadwritestatus = 1;
      chprintf(chp, "Error at %d, expected %d but read %d.\r\n", counter, ubWritedata_8b + counter, ubReaddata_8b);
    }
    counter++;
  }

  tmStopMeasurement(&tm);
  uint32_t check_ms = RTT2MS(tm.last);

  //FIXME time this
  if (!uwReadwritestatus) {
    chprintf(chp, "SDRAM read and check completed successfully in %dms.\r\n", check_ms);
  }

}

static void cmd_sdram(BaseSequentialStream *chp, int argc, char *argv[]) {
  uint32_t counter = 0;
  uint8_t ubWritedata_8b = 0x3C, ubReaddata_8b = 0;
  uint32_t uwReadwritestatus = 0;
  TimeMeasurement tm;


  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: sdram\r\n");
    return;
  }

  tmObjectInit(&tm);

  tmStartMeasurement(&tm);

//  /* Erase SDRAM memory */
//  for (counter = 0; counter < IS42S16400J_SIZE; counter++)
//  {
//    *(__IO uint8_t*) (SDRAM_BANK_ADDR + counter) = (uint8_t)0x0;
//  }

  /* Write data value to all SDRAM memory */
  for (counter = 0; counter < IS42S16400J_SIZE; counter++)
  {
    *(__IO uint8_t*) (SDRAM_BANK_ADDR + counter) = (uint8_t)(ubWritedata_8b + counter);
  }

  tmStopMeasurement(&tm);
  uint32_t write_ms = RTT2MS(tm.last);

  tmStartMeasurement(&tm);

  /* Read back SDRAM memory */
  counter = 0;
  while ((counter < IS42S16400J_SIZE))
  {
    ubReaddata_8b = *(__IO uint8_t*)(SDRAM_BANK_ADDR + counter);
    counter++;
  }

  tmStopMeasurement(&tm);
  uint32_t read_ms = RTT2MS(tm.last);

  /* Read back SDRAM memory and check content correctness*/
  counter = 0;
  uwReadwritestatus = 0;
  while ((counter < IS42S16400J_SIZE) && (uwReadwritestatus == 0))
  {
    ubReaddata_8b = *(__IO uint8_t*)(SDRAM_BANK_ADDR + counter);
    if ( ubReaddata_8b != (uint8_t)(ubWritedata_8b + counter))
    {
      uwReadwritestatus = 1;
      chprintf(chp, "Error at %d, expected %d but read %d.\r\n", counter, ubWritedata_8b + counter, ubReaddata_8b);
    }
    counter++;
  }



  //FIXME time this
  if (!uwReadwritestatus) {
    chprintf(chp, "SDRAM test completed successfully, writing entire memory took %dms, reading it took %dms.\r\n", write_ms, read_ms);
  }

}

static const ShellCommand commands[] = {
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"test", cmd_test},
  {"sdram", cmd_sdram},
  {"reset", cmd_reset},
  {"write", cmd_write},
  {"check", cmd_check},
  {"erase", cmd_erase},
  {"selfrefresh", cmd_selfrefresh},
  {"normal", cmd_normal},
  {NULL, NULL}
};

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SD1,
  commands
};

/*===========================================================================*/
/* Initialization and main thread.                                           */
/*===========================================================================*/

/*
 * Application entry point.
 */
int main(void) {
  Thread *shelltp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Shell manager initialization.
   */
  shellInit();

  //Configure pins for USART1
  palSetPadMode(GPIOA, GPIOA_PIN9, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, GPIOA_PIN10, PAL_MODE_ALTERNATE(7));

  /*
   * Initializes serial on USART1 with default configuration
   */
  sdStart(&SD1, NULL);

  /*
   * Creating the blinker threads.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1),
                    NORMALPRIO + 10, Thread1, NULL);
  chThdCreateStatic(waThread2, sizeof(waThread2),
                    NORMALPRIO + 10, Thread2, NULL);

  /*
   * Initialise SDRAM, board.h has already configured GPIO correctly (except that ST example uses 50MHz not 100MHz?)
   */
  SDRAM_Init();


  /*
   * Normal main() thread activity, in this demo it just performs
   * a shell respawn upon its termination.
   */
  while (TRUE) {
    if (!shelltp) {
      /* Spawns a new shell.*/
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    }
    else {
      /* If the previous shell exited.*/
      if (chThdTerminated(shelltp)) {
        /* Recovers memory of the previous shell.*/
        chThdRelease(shelltp);
        shelltp = NULL;
      }
    }
    chThdSleepMilliseconds(500);
  }
}
