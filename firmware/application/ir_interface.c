#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <signal.h> //sig_atomic_t
#include "ir_interface.h"
#include "systick_local.h"

#define GPIO_PORT    GPIOB
#define GPIO_DATA_IN GPIO4
#define GPIO_DATA_CLK GPIO3

/// todo:comment
#define TIM_PULSE_GEN_PRESCALER 7999
#define TIM_PULSE_GEN_OCCR      60
#define TIM_PULSE_GEN_ARR       65

/// todo: comment
#define TIM_CLK_GEN_PRESCALER   39
#define TIM_CLK_GEN_OCCR        100
#define TIM_CLK_GEN_ARR         199


/// todo comment
#define GPIOB_IDR_ADDR (GPIOB+0x08)

#define DMM_DATA_BITS_LEN (IR_DATA_LEN*8)


/// todo comment
static void ir_itf_generate_start_pulse(void);
/// todo comment
static void configure_tim_oc(const uint32_t timer_peripheral, enum tim_oc_id oc_channel, const uint16_t ccr_value);
/// todo comment
static void configure_tim(const uint32_t timer_peripheral);
/// todo comment
static void configure_exti_for_data_ready_signal(void);


// todo : comment of private variables
static volatile uint32_t* dataInBit = NULL;
// todo: comment
static volatile uint8_t bitNo = 0;
static volatile uint8_t byteNo = 0;
static uint8_t* pActiveBuf = NULL;

// todo state
// 0 - ready
// 1 - working
#define STATE_READY 0
#define STATE_WORKING 1
#define STATE_DONE 2
static volatile sig_atomic_t dmmCommState = STATE_READY;

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

    if ((NULL != buffer) && (len >= IR_DATA_LEN)) {
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
        for (int byteNo = 0; byteNo < IR_DATA_LEN; ++byteNo) {
            for (int bitNo = 0; bitNo < 8; ++bitNo) {
                gpio_set(GPIO_PORT, GPIO_DATA_CLK);
                //nops_wait();nops_wait();nops_wait();nops_wait();
                st_delay_ms(1);

                gpio_clear(GPIO_PORT, GPIO_DATA_CLK);
                uint16_t bitVal = gpio_get(GPIO_PORT, GPIO_DATA_IN); // 0 or 1
                // insert just read bit into right place of buffer
                // this is branching less replacement for: if(bitVal == 1) buffer[byteNo] |= (1<<bitNo); else buffer[byteNo] &= ~(1<<bitNo);
                buffer[byteNo] ^= (-bitVal ^ buffer[byteNo]) & (1 << bitNo);

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

    // enable clock for TIM2
    rcc_periph_clock_enable(RCC_TIM2);

    // reset tim2 peripheral to defaults
    rcc_periph_reset_pulse(RST_TIM2);

    // configure input pin, assume external pull-down or pull-up
    gpio_set_mode(GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_DATA_IN);

    // configure clk pin -> alternate function -> using as PWM output
    gpio_set_mode(GPIO_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_DATA_CLK);
    // remap to TIM2-CH2 to PB3
    gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, AFIO_MAPR_TIM2_REMAP_PARTIAL_REMAP1);

    timer_reset(TIM2);


    // map vale of (bit) of DATA_IN pin to variable (bit banding) | fixme: magic number!
    dataInBit = &BBIO_PERIPH((GPIOB_IDR_ADDR), 4);
}


void ir_itf_start_read_nb(uint8_t* const buffer, const size_t len) {
    if (NULL == buffer || len < IR_DATA_LEN || STATE_WORKING == dmmCommState) {
        return;
    }


    bitNo = 0;
    byteNo = 0;
    pActiveBuf = buffer;

    dmmCommState = STATE_WORKING;
    ir_itf_generate_start_pulse();
}

int ir_itf_get_status(void) {
    int isWorking = 1;

    if (STATE_DONE == dmmCommState) {
        dmmCommState = STATE_READY;
        isWorking = 2;
    } else if (STATE_READY == dmmCommState) {
        isWorking = 0;
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

static void configure_tim(const uint32_t timer_peripheral) {
    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
}

static void configure_exti_for_data_ready_signal(void) {
    // configure the EXTI subsystem
    exti_select_source(EXTI4, GPIO_PORT);
    // rising edge when receiving signal from DMM
    exti_set_trigger(EXTI4, EXTI_TRIGGER_RISING);
    exti_enable_request(EXTI4);

    // enable EXTI4 interrupt
    nvic_clear_pending_irq(NVIC_EXTI4_IRQ);
    nvic_enable_irq(NVIC_EXTI4_IRQ);
}

static void ir_itf_generate_start_pulse(void) {
    // need to configure timer into PWM with one shot mode to generate the
    nvic_disable_irq(NVIC_TIM2_IRQ);

    // reset tim2 peripheral to defaults
    timer_reset(RST_TIM2);

    // TIM2 uses APB1 and with current settings it has prescaler set to 2, but clock used by TIM2 is doubled in that
    // case
    configure_tim(TIM2);
    timer_set_prescaler(TIM2, TIM_PULSE_GEN_PRESCALER);
    timer_enable_preload(TIM2);
    timer_one_shot_mode(TIM2);
    timer_set_period(TIM2, TIM_PULSE_GEN_ARR);
    configure_tim_oc(TIM2, TIM_OC2, TIM_PULSE_GEN_OCCR);
    // software generate UpdateEvent to apply values in shadowed registers
    timer_generate_event(TIM2, TIM_EGR_UG);
    TIM_SR(TIM2) = 0; // clearing all flags in status register (clear on write '0')

    timer_enable_counter(TIM2);

    // Counter works in one shot mode, which means: UE will occur and counter will be disabled, but output compare stage
    // will set HIGH state on pin due to reloaded values in registers. This will turn on IR LED - what is undesirable.
    // This write sets 0 on compare register to force output level to be low when UE occur.
    // Because counter is enabled this write take effect only on UpdateEvent.
    timer_set_oc_value(TIM2, TIM_OC2, 0);

    // configure exti on DATA INPUT pin on rising edge -> this will be an event when DMM is ready to transmit data
    configure_exti_for_data_ready_signal();
}



__attribute__((interrupt)) void tim2_isr(void) {
    if (true == timer_interrupt_source(TIM2, TIM_SR_CC2IF)) { // Falling edge of CLK signal -> edge of sampling
        timer_clear_flag(TIM2, TIM_SR_CC2IF);
        //
        // Note about assumption: byteNo and bitNo must be set to 0 prior first interrupt occur
        //
        uint16_t bitVal = *dataInBit; // Read 0 or 1 from bit-band region

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
            if (byteNo >= IR_DATA_LEN) {
                // that is all, force output to be low, disable counter, set appropriate flag that informs about finished
                // data reading
                timer_set_oc_mode(TIM2, TIM_OC2, TIM_OCM_FORCE_LOW);
                timer_disable_irq(TIM2, TIM_DIER_CC2IE);
                timer_disable_counter(TIM2);

                nvic_disable_irq(NVIC_TIM2_IRQ);
                nvic_clear_pending_irq(NVIC_TIM2_IRQ);

                dmmCommState = STATE_DONE;
            }
        }
    } // TIM_SR_CC2IF
} // tim2_isr()

__attribute__((interrupt)) void exti4_isr(void) {
    exti_reset_request(EXTI4);
    // disable exti
    exti_disable_request(EXTI4);
    nvic_disable_irq(NVIC_EXTI4_IRQ);
    nvic_clear_pending_irq(NVIC_EXTI4_IRQ);

    // re-setup TIMER to generate clock for 128 bits of data from DMM
    // data will be read on falling edge
    timer_disable_counter(TIM2);
    nvic_disable_irq(NVIC_TIM2_IRQ);

    timer_set_prescaler(TIM2, TIM_CLK_GEN_PRESCALER);
    timer_set_period(TIM2, TIM_CLK_GEN_ARR);
    timer_set_oc_value(TIM2, TIM_OC2, TIM_CLK_GEN_OCCR);
    timer_continuous_mode(TIM2);

    // software generate UpdateEvent to apply values in shadowed registers
    timer_generate_event(TIM2, TIM_EGR_UG);
    TIM_SR(TIM2) = 0; // clearing all flags in status register (clear on write '0')
    timer_enable_irq(TIM2, TIM_DIER_CC2IE);

    nvic_clear_pending_irq(NVIC_TIM2_IRQ);
    nvic_enable_irq(NVIC_TIM2_IRQ);

    // start counting
    timer_enable_counter(TIM2);
}
