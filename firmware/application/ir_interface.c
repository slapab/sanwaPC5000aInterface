#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <signal.h> //sig_atomic_t
#include "ir_interface.h"
#include "systick_local.h"

#if INTERFACE_VER1 == USING_INTERFACE_VER

#define GPIO_PORT               GPIOB
#define GPIO_DATA_CLK           GPIO3
#define GPIO_DATA_IN            GPIO4
/// Number of pin that is used to receive data
#define GPIO_DATA_IN_PIN_NO     4
/// Used timer to generate start impulse and clock signal. See doc. of TIMER of libopencm3.
#define USED_TIMER_PERIPH       TIM2
/// Output compare channel number. See doc. of TIMER of libopencm3.
#define USED_TIMER_OC_CHANNEL   TIM_OC2
/// Timer Compare/Capture interrupt enable bit. See of "TIMx_DIER Timer DMA and Interrupt Enable Values" of libopencm3.
#define USED_TIMER_DIER_CCIE    TIM_DIER_CC2IE
/// NVIC IRQ number that is assigned to used timer. See doc. of NVIC of libopencm3.
#define USED_TIMER_NVIC_IRQ     NVIC_TIM2_IRQ
/// EXTI source that is using to detect the DMM readiness. See doc. of EXIT of libopencm3.
#define USED_EXTI_SOURCE        EXTI4
/// NVIC IRQ number that is assigned to used EXTI source. See doc. of NVIC of libopencm3.
#define USED_EXTI_NVIC_IRQ      NVIC_EXTI4_IRQ
/// Defines the EXTI trigger type. See doc. of EXTI of libopencm3.
#define USED_EXTI_TRIGGER_TYPE  EXTI_TRIGGER_RISING

#endif

/**
 * Timer's registers configuration values which are using to generate 10ms-long level on the output pin.
 *
 * These values were counted with assumptions:
 * - frequency of the internal timer's clock: 48 MHz
 * - runs in pwm - mode 1
 * - required high level time - 10ms
 * - period as short as possible (high level time plus something small to get integer value)
*/
#define TIM_PULSE_GEN_PRESCALER 7999
#define TIM_PULSE_GEN_OCCR      60
#define TIM_PULSE_GEN_ARR       65

/**
 * Timer's registers configuration values which are using to generate 'clk' signal on the output pin.
 *
 * These value were counted with assumptions:
 * - frequency of the internal timer's clock: 48 MHz
 * - runs in pwm - mode 1
 * - duty: 50%
 * - th = tl = not less than 2us
 */
#define TIM_CLK_GEN_PRESCALER   39
#define TIM_CLK_GEN_OCCR        100
#define TIM_CLK_GEN_ARR         199


/// Defines offset of the Input Data Register (IDR) from the GPIO base address.
#define GPIO_IDR_OFFSET 0x08
/// Defines address of the Input Data REgister (IDR) for given GPIO port.
#define GPIO_IDR_ADDR(gpio_port) ((gpio_port)+GPIO_IDR_OFFSET)
/// How many bits need to read from DMM during one transaction.
#define DMM_DATA_BITS_LEN (IR_DATA_BYTES*8)
/// Macro that assembles all possible interrupt flags that can be cleared in TIM's status register
#define TIM_SR_ALL_INT_FLAGS (TIM_SR_BIF | TIM_SR_CC1IF | TIM_SR_CC2IF | TIM_SR_CC3IF | TIM_SR_CC4IF | \
                              TIM_SR_COMIF | TIM_SR_TIF | TIM_SR_UIF)


/// Configures timer to generate start impulse.
static void ir_itf_generate_start_pulse(void);

/** Configures Output Compare of given timer to use PWM1 mode.
 *
 *  @param timer_peripheral  Timer register address base (see doc of libopencm3).
 *  @param oc_channel        OC channel designators TIM_OCx where x=1..4 (see doc. of libopencm3).
 *  @param ccr_value         Compare value.
 */
static void configure_tim_oc(const uint32_t timer_peripheral, enum tim_oc_id oc_channel, const uint16_t ccr_value);

/// Configures exti to catch signal from the DMM when it is ready to transmit data.
static void configure_exti_for_data_ready_signal(void);


/// This variable points to the bit-band region which stores the value of current bit on 'data in' pin.
static volatile uint32_t* dataInBit = NULL;
/// Using in the timer interrupt to track the receiving bit number.
static volatile uint8_t bitNo = 0;
/// Using in the timer interrupt to track the receiving byte number.
static volatile uint8_t byteNo = 0;
/// Points to the active buffer where receiving data are storing.
static uint8_t* pActiveBuf = NULL;


static volatile sig_atomic_t dmmCommState = IR_ITF_READY;

void ir_itf_init_blocking(void) {
    rcc_periph_clock_enable(RCC_GPIOB);

    // configure input pin, assume external pull-down or pull-up
    gpio_set_mode(GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_DATA_IN);
    // configure clk pin
    gpio_set_mode(GPIO_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO_DATA_CLK);
    gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
}


bool ir_itf_read_blocking(uint8_t* const buffer, const size_t len) {
    bool retval = false;

    if ((NULL != buffer) && (len >= IR_DATA_BYTES)) {
    do {
        // preconditions -> IR receiver must giving at this moment '0'
        if (0 != gpio_get(GPIO_PORT, GPIO_DATA_IN)) {
            break;
        }

        // generate 10ms impulse on IR LED
        gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
        asm("nop"); asm("nop"); asm("nop");
        gpio_set(GPIO_PORT, GPIO_DATA_CLK);
        st_delay_ms(11);
        gpio_clear(GPIO_PORT, GPIO_DATA_CLK);

        // wait for '1' from dmm but not more than 200ms
        systick_t tpStart = st_get_ticks();
        while(0 == gpio_get(GPIO_PORT, GPIO_DATA_IN) && st_get_time_duration(tpStart) <= 200/*ms*/) {;}
        // check if DMM send response not timeout
        if (0 == gpio_get(GPIO_PORT, GPIO_DATA_IN)) {
            break;
        }

        // start generating 128 impulses on data clock and read data on falling edge
        for (int byteNO = 0; byteNO < IR_DATA_BYTES; ++byteNO) {
            for (int bitNO = 0; bitNO < 8; ++bitNO) {
                gpio_set(GPIO_PORT, GPIO_DATA_CLK);
                //nops_wait();nops_wait();nops_wait();nops_wait();
                st_delay_ms(1);

                gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
                uint16_t bitVal = gpio_get(GPIO_PORT, GPIO_DATA_IN); // 0 or 1
                // insert just read bit into right place of buffer
                // this is branching less replacement for: if(bitVal == 1) buffer[byteNo] |= (1<<bitNo); else buffer[byteNo] &= ~(1<<bitNo);
                buffer[byteNO] ^= (-bitVal ^ buffer[byteNO]) & (1 << bitNO);

//                nops_wait();nops_wait();nops_wait();nops_wait();
                st_delay_ms(1);
            }
        }
        retval = true;
    } while (0);
    } // checking function args

    return retval;
}


void ir_itf_init_nb(void) {
    //  enable clock for gpio pins used as an ir interface
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_AFIO);

    // enable clock for USED_TIMER_PERIPH
    rcc_periph_clock_enable(RCC_TIM2);

    // reset tim2 peripheral to defaults
    rcc_periph_reset_pulse(RST_TIM2);

    // configure input pin, assume external pull-down or pull-up
    gpio_set_mode(GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_DATA_IN);

    // configure clk pin -> alternate function -> using as PWM output
    gpio_set_mode(GPIO_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_DATA_CLK);

#if INTERFACE_VER1 == USING_INTERFACE_VER
    // remap TIM2-CH2 to PB3
    gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, AFIO_MAPR_TIM2_REMAP_PARTIAL_REMAP1);
#endif

    timer_reset(USED_TIMER_PERIPH);

    // maps bit vale of DATA_IN pin to variable (bit banding)
    dataInBit = &BBIO_PERIPH((GPIO_IDR_ADDR(GPIO_PORT)), GPIO_DATA_IN_PIN_NO);
}


void ir_itf_start_read_nb(uint8_t* const buffer, const size_t len) {
    if (NULL == buffer || len < IR_DATA_BYTES || IR_ITF_WORKING == dmmCommState) {
        return;
    }


    bitNo = 0;
    byteNo = 0;
    pActiveBuf = buffer;

    dmmCommState = IR_ITF_WORKING;
    ir_itf_generate_start_pulse();
}

ir_itf_state_type ir_itf_get_status(void) {
    ir_itf_state_type isWorking = IR_ITF_WORKING;

    if (IR_ITF_DONE == dmmCommState) {
        dmmCommState = IR_ITF_READY;
        isWorking = IR_ITF_DONE;
    } else if (IR_ITF_READY == dmmCommState) {
        isWorking = IR_ITF_READY;
    }

    return isWorking;
}

static void configure_tim_oc(const uint32_t timer_peripheral, enum tim_oc_id oc_channel,  const uint16_t ccr_value) {
    timer_disable_oc_output(timer_peripheral, oc_channel);
    timer_set_oc_mode(timer_peripheral, oc_channel, TIM_OCM_PWM1);
    timer_enable_oc_preload(timer_peripheral, oc_channel);
    timer_set_oc_value(timer_peripheral, oc_channel, ccr_value);
    timer_enable_oc_output(timer_peripheral, oc_channel);
}


static void configure_exti_for_data_ready_signal(void) {
    // configure the EXTI subsystem
    exti_select_source(USED_EXTI_SOURCE, GPIO_PORT);
    // rising edge when receiving signal from DMM
    exti_set_trigger(USED_EXTI_SOURCE, USED_EXTI_TRIGGER_TYPE);
    exti_enable_request(USED_EXTI_SOURCE);

    // enable USED_EXTI_SOURCE interrupt
    nvic_clear_pending_irq(USED_EXTI_NVIC_IRQ);
    nvic_enable_irq(USED_EXTI_NVIC_IRQ);
}

static void ir_itf_generate_start_pulse(void) {
    // need to configure timer into PWM with one shot mode to generate the
    nvic_disable_irq(USED_TIMER_NVIC_IRQ);

    // reset USED_TIMER_PERIPH peripheral to defaults
    timer_reset(USED_TIMER_PERIPH);

    // USED_TIMER_PERIPH uses APB1 and with current settings it has prescaler set to 2, but clock used by TIM2 is doubled in that
    // case
    timer_set_mode(USED_TIMER_PERIPH, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(USED_TIMER_PERIPH, TIM_PULSE_GEN_PRESCALER);
    timer_enable_preload(USED_TIMER_PERIPH);
    timer_one_shot_mode(USED_TIMER_PERIPH);
    timer_set_period(USED_TIMER_PERIPH, TIM_PULSE_GEN_ARR);
    configure_tim_oc(USED_TIMER_PERIPH, USED_TIMER_OC_CHANNEL, TIM_PULSE_GEN_OCCR);
    // software generate UpdateEvent to apply values in shadowed registers
    timer_generate_event(USED_TIMER_PERIPH, TIM_EGR_UG);
    // clearing all flags in status register (clear on write '0')
    timer_clear_flag(USED_TIMER_PERIPH, TIM_SR_ALL_INT_FLAGS);

    timer_enable_counter(USED_TIMER_PERIPH);

    // Counter works in one shot mode, which means: UE will occur and counter will be disabled, but output compare stage
    // will set HIGH state on pin due to reloaded values in registers. This will turn on IR LED - what is undesirable.
    // This write sets 0 on compare register to force output level to be low when UE occur.
    // Because counter is enabled this write take effect only on UpdateEvent.
    timer_set_oc_value(USED_TIMER_PERIPH, USED_TIMER_OC_CHANNEL, 0);

    // configure exti on DATA INPUT pin on rising edge -> this will be an event when DMM is ready to transmit data
    configure_exti_for_data_ready_signal();
}


/**
 * On each compare match only when generating CLK signal. This interrupt represents the falling edge of clock which
 * is also a data sampling edge.
 */
__attribute__((interrupt)) void tim2_isr(void) {
    if (true == timer_interrupt_source(USED_TIMER_PERIPH, TIM_SR_CC2IF)) { // Falling edge of CLK signal -> edge of sampling
        timer_clear_flag(USED_TIMER_PERIPH, TIM_SR_CC2IF);
        //
        // Note about assumption: byteNo and bitNo must be set to 0 prior first interrupt occur
        //

        // Read 0 or 1 from bit-band region of IDR of given GPIO pin
        uint16_t bitVal = *dataInBit;

        // insert just read bit into right place of buffer
        // this is branching less replacement for: if(bitVal == 1) buffer[byteNo] |= (1<<bitNo); else buffer[byteNo] &= ~(1<<bitNo);
        // see https://graphics.stanford.edu/~seander/bithacks.html
        pActiveBuf[byteNo] ^= (-bitVal ^ pActiveBuf[byteNo]) & (1 << bitNo);

        // handle bit counting
        ++bitNo;
        if (bitNo >= 8) {
            bitNo = 0;
            ++byteNo;
            // check if that was a last byte
            if (byteNo >= IR_DATA_BYTES) {
                // that is all, force output to be low, disable counter, set appropriate flag that informs about finished
                // data reading
                timer_set_oc_mode(USED_TIMER_PERIPH, USED_TIMER_OC_CHANNEL, TIM_OCM_FORCE_LOW);
                timer_disable_irq(USED_TIMER_PERIPH, USED_TIMER_DIER_CCIE);
                timer_disable_counter(USED_TIMER_PERIPH);

                nvic_disable_irq(USED_TIMER_NVIC_IRQ);
                nvic_clear_pending_irq(USED_TIMER_NVIC_IRQ);

                dmmCommState = IR_ITF_DONE;
            }
        }
    } // TIM_SR_CC2IF
} // tim2_isr()


/**
 * Interrupt occurs when DMM indicates (by turning its IR LED on) when it is read to transmit data.
 */
__attribute__((interrupt)) void exti4_isr(void) {
    exti_reset_request(USED_EXTI_SOURCE);
    // disable exti
    exti_disable_request(USED_EXTI_SOURCE);
    nvic_disable_irq(USED_EXTI_NVIC_IRQ);
    nvic_clear_pending_irq(USED_EXTI_NVIC_IRQ);

    // re-setup TIMER to generate clock for 128 bits of data from DMM
    // data will be read on falling edge
    timer_disable_counter(USED_TIMER_PERIPH);
    nvic_disable_irq(USED_TIMER_NVIC_IRQ);

    timer_set_prescaler(USED_TIMER_PERIPH, TIM_CLK_GEN_PRESCALER);
    timer_set_period(USED_TIMER_PERIPH, TIM_CLK_GEN_ARR);
    timer_set_oc_value(USED_TIMER_PERIPH, USED_TIMER_OC_CHANNEL, TIM_CLK_GEN_OCCR);
    timer_continuous_mode(USED_TIMER_PERIPH);

    // software generate UpdateEvent to apply values in shadowed registers
    timer_generate_event(USED_TIMER_PERIPH, TIM_EGR_UG);
    // clearing all flags in status register (clear on write '0')
    timer_clear_flag(USED_TIMER_PERIPH, TIM_SR_ALL_INT_FLAGS);
    // Trigger interrupt on compare match -> this will be the falling edge of the clock signal -> sampling edge
    timer_enable_irq(USED_TIMER_PERIPH, USED_TIMER_DIER_CCIE);

    nvic_clear_pending_irq(USED_TIMER_NVIC_IRQ);
    nvic_enable_irq(USED_TIMER_NVIC_IRQ);

    // start counting
    timer_enable_counter(USED_TIMER_PERIPH);
} // exti4_isr()
