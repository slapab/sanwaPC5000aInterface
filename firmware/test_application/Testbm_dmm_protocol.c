#include "bm_dmm_protocol.h"
#include "unity.h"

// AC 13.722V
static const data_resp_pkt example_voltageReading1 = {
        .header = {.dle = 0x10, .stx = 0x02, .cmd = 0x00, .dataLen = BM_NORMAL_PACKET_DATA_LENGTH},
        .func[0] = 0x05 /* AC V */, 0x00, 0x00, 0x00,
        .valSign = 0x20, /*no sign*/
        .asciiAndTailLong = {
                .d1 = 0x31, 0x2E, 0x33, 0x37, 0x37, 0x32, 0x20, 0x45, 0x2B, 0x31,
                .pktTail = {.chkSum = 0x44, .dle = 0x10, .etx = 0x03}
        }
};

// DC -382.345V
static const data_resp_pkt example_voltageReading2 = {
        .header = {.dle = 0x10, .stx = 0x02, .cmd = 0x00, .dataLen = BM_NORMAL_PACKET_DATA_LENGTH},
        .func[0] = 0x06 /* DC V */, 0x00, 0x00, 0x00,
        .valSign = 0x2D, /*negative value*/
        .asciiAndTailLong = {
                .d1 = 0x33, 0x2E, 0x38, 0x32, 0x33, 0x34, 0x35, 0x45, 0x2B, 0x32,
                .pktTail = {.chkSum = 0x52, .dle = 0x10, .etx = 0x03}
        }
};

// Over Limit packet when measuring DC V
static const data_resp_pkt example_OverLimitPkt = {
        .header = {.dle = 0x10, .stx = 0x02, .cmd = 0x01, .dataLen = BM_OL_PACKET_DATA_LENGTH},
        .func[0] = 0x06 /* DC V */, 0x00, 0x00, 0x00,
        .valSign = 0x20, /*DMM doesn't print minus*/
        .asciiAndTailShort = {
                .oChar = 0x4F,
                .lChar = 0x4C,
                .pktTail = {.chkSum = 0x25, .dle = 0x10, .etx = 0x03}
        }
};

// forward declarations of static functions
void bm_calculate_pkt_check_sum(data_resp_pkt* const pRespPack);
void bm_fill_pkt_constants(data_resp_pkt* const pRespPack);
uint8_t convert_digit_segs_to_val(uint8_t segments);


void test_bm_calculate_pkt_check_sum(void) {
    data_resp_pkt data_pkt = example_voltageReading1;

    // intentionally change checksum value
    data_pkt.asciiAndTailLong.pktTail.chkSum = 0x00;
    // call function to calculate the checksum and save it inplace
    bm_calculate_pkt_check_sum(&data_pkt);
    // test if calculate checksum is equal to expected one
    TEST_ASSERT_EQUAL_UINT8(example_voltageReading1.asciiAndTailLong.pktTail.chkSum,
                      data_pkt.asciiAndTailLong.pktTail.chkSum);


    // ----------------------------------------------------------------------------------------------------------------
    // run with another packet
    data_pkt = example_voltageReading2;
    // intentionally change checksum value
    data_pkt.asciiAndTailLong.pktTail.chkSum = 0x00;
    bm_calculate_pkt_check_sum(&data_pkt);
    TEST_ASSERT_EQUAL_UINT8(example_voltageReading2.asciiAndTailLong.pktTail.chkSum,
                      data_pkt.asciiAndTailLong.pktTail.chkSum);


    // ----------------------------------------------------------------------------------------------------------------
    // Test for Over Limit packet
    data_pkt = example_OverLimitPkt;
    // intentionally change checksum value
    data_pkt.asciiAndTailShort.pktTail.chkSum = 0x00;
    bm_calculate_pkt_check_sum(&data_pkt);
    TEST_ASSERT_EQUAL_UINT8(example_OverLimitPkt.asciiAndTailShort.pktTail.chkSum,
                      data_pkt.asciiAndTailShort.pktTail.chkSum);
}

void test_bm_fill_pkt_constants(void) {
    // perform tests for data readings
    {
        data_resp_pkt data_pkt = {0};
        data_pkt.header.dataLen = BM_NORMAL_PACKET_DATA_LENGTH; // indicate that this packet stores data readings
        bm_fill_pkt_constants(&data_pkt);
        TEST_ASSERT_EQUAL_UINT8(0x10, data_pkt.header.dle);
        TEST_ASSERT_EQUAL_UINT8(0x02, data_pkt.header.stx);
        TEST_ASSERT_EQUAL_UINT8(0x00, data_pkt.header.cmd);
        TEST_ASSERT_EQUAL_UINT8(0x2E, data_pkt.asciiAndTailLong.constDotChar);
        TEST_ASSERT_EQUAL_UINT8(0x45, data_pkt.asciiAndTailLong.constEChar);
        TEST_ASSERT_EQUAL_UINT8(0x10, data_pkt.asciiAndTailLong.pktTail.dle);
        TEST_ASSERT_EQUAL_UINT8(0x03, data_pkt.asciiAndTailLong.pktTail.etx);
    }

    // tests for OverLimit packet
    {
        data_resp_pkt data_pkt = {0};
        // perform tests for data readings
        data_pkt.header.dataLen = BM_OL_PACKET_DATA_LENGTH; // indicate that this packet is OverLimit packet
        bm_fill_pkt_constants(&data_pkt);
        TEST_ASSERT_EQUAL_UINT8(0x10, data_pkt.header.dle);
        TEST_ASSERT_EQUAL_UINT8(0x02, data_pkt.header.stx);
        TEST_ASSERT_EQUAL_UINT8(0x01, data_pkt.header.cmd);   // 0x01 - typical for OV packet (according to the doc)
        TEST_ASSERT_EQUAL_UINT8(0x10, data_pkt.asciiAndTailShort.pktTail.dle);
        TEST_ASSERT_EQUAL_UINT8(0x03, data_pkt.asciiAndTailShort.pktTail.etx);
    }
}

void test_convert_digit_segs_to_val(void) {
    // bits assigned to digit's segments
    const uint8_t a = (1 << 3);
    const uint8_t b = (1 << 7);
    const uint8_t c = (1 << 5);
    const uint8_t d = (1 << 4);
    const uint8_t e = (1 << 1);
    const uint8_t f = (1 << 2);
    const uint8_t g = (1 << 6);

    const uint8_t digit_1 = b+c;
    const uint8_t digit_2 = a+b+g+e+d;
    const uint8_t digit_3 = a+b+g+c+d;
    const uint8_t digit_4 = f+g+c+b;
    const uint8_t digit_5 = a+f+g+c+d;
    const uint8_t digit_6 = a+f+e+d+c+g;
    const uint8_t digit_7 = a+b+c;
    const uint8_t digit_8 = a+b+c+d+e+f+g;
    const uint8_t digit_9 = a+b+c+d+f+g;
    const uint8_t digit_0 = a+b+c+d+f+e;
    const uint8_t digit_L = f+e+d;
    const uint8_t digit_empty = 0;

    TEST_ASSERT_EQUAL_UINT8(0x31, convert_digit_segs_to_val(digit_1));
    TEST_ASSERT_EQUAL_UINT8(0x32, convert_digit_segs_to_val(digit_2));
    TEST_ASSERT_EQUAL_UINT8(0x33, convert_digit_segs_to_val(digit_3));
    TEST_ASSERT_EQUAL_UINT8(0x34, convert_digit_segs_to_val(digit_4));
    TEST_ASSERT_EQUAL_UINT8(0x35, convert_digit_segs_to_val(digit_5));
    TEST_ASSERT_EQUAL_UINT8(0x36, convert_digit_segs_to_val(digit_6));
    TEST_ASSERT_EQUAL_UINT8(0x37, convert_digit_segs_to_val(digit_7));
    TEST_ASSERT_EQUAL_UINT8(0x38, convert_digit_segs_to_val(digit_8));
    TEST_ASSERT_EQUAL_UINT8(0x39, convert_digit_segs_to_val(digit_9));
    TEST_ASSERT_EQUAL_UINT8(0x30, convert_digit_segs_to_val(digit_0));
    TEST_ASSERT_EQUAL_UINT8(0x4C, convert_digit_segs_to_val(digit_L));
    TEST_ASSERT_EQUAL_UINT8(0x20, convert_digit_segs_to_val(digit_empty));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bm_calculate_pkt_check_sum);
    RUN_TEST(test_bm_fill_pkt_constants);
    RUN_TEST(test_convert_digit_segs_to_val);
    return UNITY_END();
}
