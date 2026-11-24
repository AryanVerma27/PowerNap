/*
 * PowerNap — Low Power Modes, Watchdog Timer (interval mode) and EnergyTrace+
 *
 * Board : MSP-EXP432P401R (Code Composer Studio + SimpleLink MSP432 SDK)
 *
 * Behaviour
 *   - Active Mode (AM): the watchdog, running as an interval timer, blinks LED1 (P1.0).
 *   - Press S1 (P1.1) / S2 (P1.4) -> toggle between Active Mode and Low Power Mode.
 *   - Low Power Mode: CPU sleeps (LPM0 or LPM3, see LPM_MODE) and LED1 only toggles
 *     on each watchdog wake-up, so the current drop is visible in EnergyTrace.
 *
 * All unused GPIOs are driven low to minimise leakage current.
 */

#include "msp.h"
#include <stdint.h>
#include <stdbool.h>

#define LPM_MODE 3            // 0 -> LPM0, 3 -> LPM3

static volatile bool state_change  = false;
static volatile bool low_pwr_state = false;

static bool EnterLowPowerMode(void);
static bool EnterActiveMode(void);
static void gpio_low_power_init(void);
static void lfxt_init(void);

/* Watchdog as interval timer, clocked from BCLK (32.768 kHz, keeps running in LPM3)
 *   IS_4 -> 2^15 / 32768 = 1 s     (Active Mode)
 *   IS_5 -> 2^13 / 32768 = 250 ms  (state-change interval)                         */
#define WDT_INTERVAL_AM   (WDT_A_CTL_PW | WDT_A_CTL_SSEL_3 | WDT_A_CTL_TMSEL | WDT_A_CTL_CNTCL | WDT_A_CTL_IS_4)
#define WDT_INTERVAL_LPM  (WDT_A_CTL_PW | WDT_A_CTL_SSEL_3 | WDT_A_CTL_TMSEL | WDT_A_CTL_CNTCL | WDT_A_CTL_IS_5)

void main(void)
{
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;      // 1. stop watchdog

    gpio_low_power_init();                           // 2. all ports output-low
    lfxt_init();                                     //    32.768 kHz crystal for BCLK

    // 3. LED1 on P1.0
    P1->DIR |=  BIT0;
    P1->OUT &= ~BIT0;
    P1->REN &= ~BIT0;

    //    Push-button on P1.4 (and P1.1), falling-edge interrupt with pull-up
    P1->DIR &= ~(BIT1 | BIT4);
    P1->REN |=  (BIT1 | BIT4);
    P1->OUT |=  (BIT1 | BIT4);
    P1->IES |=  (BIT1 | BIT4);
    P1->IFG &= ~(BIT1 | BIT4);
    P1->IE  |=  (BIT1 | BIT4);
    NVIC_EnableIRQ(PORT1_IRQn);

    // 5. watchdog as interval timer
    WDT_A->CTL = WDT_INTERVAL_AM;
    NVIC_EnableIRQ(WDT_A_IRQn);

    __enable_irq();

    // 4. AM <-> LPM finite state machine
    while (1)
    {
        if (state_change)
        {
            state_change = false;
            if (low_pwr_state)
                low_pwr_state = EnterLowPowerMode();
            else
                low_pwr_state = !EnterActiveMode();
        }

        if (low_pwr_state)
            __WFI();                                 // sleep until an interrupt
    }
}

/* Push-button: request a state change */
void PORT1_IRQHandler(void)
{
    uint8_t flags = P1->IFG;
    P1->IFG &= ~flags;

    if (flags & (BIT1 | BIT4))
    {
        low_pwr_state = !low_pwr_state;
        state_change  = true;
        SCB->SCR &= ~SCB_SCR_SLEEPONEXIT_Msk;        // make sure main() runs
    }
}

/* 6. Watchdog interval ISR: blink in AM, toggle in LPM */
void WDT_A_IRQHandler(void)
{
    P1->OUT ^= BIT0;
    if (!low_pwr_state)
    {
        volatile int i;
        for (i = 0; i < 10000; i++) { }
        P1->OUT ^= BIT0;                             // short blink in Active Mode
    }
}

/* 7. Active Mode: all clocks on, CPU not in deep sleep */
static bool EnterActiveMode(void)
{
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    WDT_A->CTL = WDT_INTERVAL_AM;
    return true;
}

/* 8. Low Power Mode: only the low-frequency crystal domain stays on (LPM3),
 *    or CPU-off with peripherals clocked (LPM0). */
static bool EnterLowPowerMode(void)
{
    WDT_A->CTL = WDT_INTERVAL_LPM;

#if LPM_MODE == 3
    while (PCM->CTL1 & PCM_CTL1_PMR_BUSY) { }
    PCM->CTL0 = PCM_CTL0_KEY_VAL | PCM_CTL0_LPMR__LPM3;  // deep-sleep target = LPM3
    while (PCM->CTL1 & PCM_CTL1_PMR_BUSY) { }
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
#else
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;                  // plain sleep = LPM0
#endif
    return true;
}

/* All GPIO ports: GPIO function, output, driven low */
static void gpio_low_power_init(void)
{
    PA->SEL0 = 0x0000; PA->SEL1 = 0x0000;
    PB->SEL0 = 0x0000; PB->SEL1 = 0x0000;
    PC->SEL0 = 0x0000; PC->SEL1 = 0x0000;
    PD->SEL0 = 0x0000; PD->SEL1 = 0x0000;
    PE->SEL0 = 0x0000; PE->SEL1 = 0x0000;

    PA->DIR = 0xFFFF; PA->OUT = 0x0000;
    PB->DIR = 0xFFFF; PB->OUT = 0x0000;
    PC->DIR = 0xFFFF; PC->OUT = 0x0000;
    PD->DIR = 0xFFFF; PD->OUT = 0x0000;
    PE->DIR = 0xFFFF; PE->OUT = 0x0000;
}

/* Start the 32.768 kHz LFXT crystal (PJ.0 / PJ.1) and route it to ACLK/BCLK */
static void lfxt_init(void)
{
    uint32_t timeout = 100000;

    PJ->SEL0 |= BIT0 | BIT1;
    PJ->SEL1 &= ~(BIT0 | BIT1);

    CS->KEY   = CS_KEY_VAL;
    CS->CTL2 |= CS_CTL2_LFXT_EN;
    while ((CS->IFG & CS_IFG_LFXTIFG) && timeout--)  // wait for oscillator to settle
        CS->CLRIFG |= CS_CLRIFG_CLR_LFXTIFG;
    CS->CTL1 = (CS->CTL1 & ~(CS_CTL1_SELA_MASK | CS_CTL1_SELB)) | CS_CTL1_SELA__LFXTCLK;
    CS->KEY   = 0;
}
