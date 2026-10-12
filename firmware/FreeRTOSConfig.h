#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* RP2040 (Cortex-M0+) FreeRTOS configuration.
 *
 * Cortex-M0 has no BASEPRI, so configMAX_SYSCALL_INTERRUPT_PRIORITY is unused
 * and the port instead requires:
 *   configPRIO_BITS == 2  and  configKERNEL_INTERRUPT_PRIORITY == 3 (lowest)
 */

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configCPU_CLOCK_HZ                      ((unsigned long)125000000)
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
/* RP2040 has 264 KB SRAM shared with the SDK's own data and the USB stack.
 * Task stacks here total 1024+2048+512+512 = 4168 words (~16.3 KB) plus four
 * TCBs, the idle task, the timer task and its queue. 96 KB leaves the SDK and
 * stdio_usb comfortable headroom; 128 KB pushed bss to ~137 KB. */
#define configTOTAL_HEAP_SIZE                   ((size_t)(80 * 1024))
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configNUM_CORES                         1

/* 1024+2048+512+512 words of task stacks plus TCBs and the idle/timer tasks
 * fit inside 128 KB with headroom. */
#define configMAX_PRIORITIES                    8

/* Memory allocation hooks are not used. */
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          0

/* Software timers. Nothing in this firmware creates one, but with
 * configUSE_TRACE_FACILITY == 0 event_groups.h defines
 * xEventGroupSetBitsFromISR as a macro over xTimerPendFunctionCallFromISR, so
 * the timer API must be compiled in or the kernel port fails to build. */
#define configUSE_TIMERS                        1
#define INCLUDE_xTimerPendFunctionCall          1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Co-routines: off. */
#define configUSE_CO_ROUTINES                   0

/* Optional API functions. */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  1

/* Cortex-M0 has 2 priority bits; the kernel must sit at the lowest. */
#define configPRIO_BITS                         2
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 3
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Expanded inside portmacro.h before task.h is available, so this must use the
 * port-level macro rather than taskDISABLE_INTERRUPTS(). */
#define configASSERT(x) if ((x) == 0) { portDISABLE_INTERRUPTS(); for (;;) {} }

/* Map the FreeRTOS port interrupt handlers onto the SDK vector table names. */
#define vPortSVCHandler     isr_svcall
#define xPortPendSVHandler  isr_pendsv
#define xPortSysTickHandler isr_systick

#endif /* FREERTOS_CONFIG_H */
