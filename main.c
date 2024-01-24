/******************************************************************************
* File Name:   main.c
*
* Description: CE220692 – PSoC 6 MCU Frequency Measurement Using TCPWM
*              ported to Modus Toolbox
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define SET                 (1u)
#define RESET               (0u)
// unused #define MAXCOUNT            (65536u)    /* 16 bit Counter */
// replaced by Counter_config.period

/*******************************************************************************
* Global Variables
*******************************************************************************/
uint8_t s_intFlag = 0;
uint32_t s_counterOverflow = 0;


/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/*******************************************************************************
* Function Name: IsrCounter()
********************************************************************************
* Summary: Interrupt service routine for Counter interrupt. This function is
* executed when a capture or an overflow event happens.
*
* Parameters:
*  None
*
* Return:
*  None
*******************************************************************************/
void IsrCounter(void)
{
    /* Get which event triggered the interrupt */
    uint32 intSource = Cy_TCPWM_GetInterruptStatusMasked(Counter_HW, Counter_NUM);

    /* Clear interrupt */
    Cy_TCPWM_ClearInterrupt(Counter_HW, Counter_NUM, intSource);
    NVIC_ClearPendingIRQ(Counter_IRQ);

    /* If the interrupt is triggered by capture event then set the flag for frequency
    calculation */
    if(intSource == CY_TCPWM_INT_ON_CC)
    {
        /* Set interrupt flag */
        s_intFlag = SET;
    }

    /* If the interrupt is triggered by an overflow event, then counting how
    many times counter overflow happened in one second */
    if(intSource == CY_TCPWM_INT_ON_TC)
    {
        s_counterOverflow++;
    }
}

/******************************************************************************
* Function Name: main()
*******************************************************************************
*
* Summary: This is the system entrance point for Cortex-M4.
* This function initializes the PSoC Components to measure the frequency and
* display the result accordingly.
*
* Parameters:
*  None
*
* Return:
*  int
*
* Side Effects:
*  None
*
******************************************************************************/
int main(void)
{
    uint32_t freq, captureVal;
    cy_rslt_t result;

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif /* #if defined (CY_DEVICE_SECURE) */

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init_fc(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
            CYBSP_DEBUG_UART_CTS,CYBSP_DEBUG_UART_RTS,CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "CE220692 – PSoC 6 MCU Frequency Measurement Using TCPWM"
           "****************** \r\n\n");

    /* This project uses 3 TCPWM Component. One is set up as a Counter which is
    used to count the rising edges. A second is set up as a PWM which generates
    1 sec time window. The third is also set up as a PWM for generating different
    frequency signal and based on the Period and Compare value it generates a
    signal with a particular frequency. */

    /* Initialize with config set in component and enable the Counter */
    Cy_TCPWM_Counter_Init(Counter_HW, Counter_NUM, &Counter_config);
    Cy_TCPWM_Enable_Multiple(Counter_HW, Counter_MASK);

    /* Hookup and enable interrupt */
    cy_stc_sysint_t mycounter= {.intrSrc = Counter_IRQ, .intrPriority=7};
    Cy_SysInt_Init(&mycounter, IsrCounter);
    NVIC_EnableIRQ(Counter_IRQ);

    /* Start the Counter */
    Cy_TCPWM_TriggerStart(Counter_HW, Counter_MASK);

    /* Initialize with config set in component and start the PWM for generating
     one second time window */
    Cy_TCPWM_Counter_Init(OneSecTimer_HW, OneSecTimer_NUM, &OneSecTimer_config);
    Cy_TCPWM_Enable_Multiple(OneSecTimer_HW, OneSecTimer_MASK);
    Cy_TCPWM_TriggerStart(OneSecTimer_HW, OneSecTimer_MASK);

    /* Initialize with config set in component and start the PWM for generating
     signal of desired frequency */
    Cy_TCPWM_PWM_Init(PWM_MeasFreq_HW, PWM_MeasFreq_NUM, &PWM_MeasFreq_config);
    Cy_TCPWM_Enable_Multiple(PWM_MeasFreq_HW, PWM_MeasFreq_MASK);
    Cy_TCPWM_TriggerStart(PWM_MeasFreq_HW, PWM_MeasFreq_MASK);

    for(;;)
    {
        static uint32_t capturePrev = 0;
        if (s_intFlag)    /* Interrupt was triggered, read the captured counter value now */
        {
            /* Get the Counter value */
            captureVal = Cy_TCPWM_Counter_GetCapture(Counter_HW, Counter_NUM);

            /* Calculate frequency */
            // was freq = s_counterOverflow * MAXCOUNT + captureVal - capturePrev;
            freq = s_counterOverflow * Counter_config.period + captureVal - capturePrev;

            s_counterOverflow = RESET;
            capturePrev = captureVal;

            /* Clear the interrupt flag */
            s_intFlag = RESET;

            /* Display frequency */
            printf("Frequency = %lu Hz \n\r", freq);
        }
    }
}


/* [] END OF FILE */
