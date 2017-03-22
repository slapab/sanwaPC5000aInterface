#include "bm_dmm_protocol.h"
#include "bm_protocol_bits_descr.h"
#include "unity.h"
#include <string.h> //memset

#define MINUS_SIGN 0x2D
#define PLUS_SIGN 0x2B
#define BM_DOT_CHAR_CONST 0x2E
#define BM_EXPONENT_CHAR_CONST 0x45
#define BM_O_CHAR 0x4F
#define BM_L_CHAR 0x4C
#define EMPTY_SIGN 0x20
#define DIGIT_L             0x4C
#define DIGIT_O             0x4F
#define DIGIT_1             0x31
#define DIGIT_2             0x32
#define DIGIT_3             0x33
#define DIGIT_4             0x34
#define DIGIT_5             0x35
#define DIGIT_6             0x36
#define DIGIT_7             0x37
#define DIGIT_8             0x38
#define DIGIT_9             0x39
#define DIGIT_0             0x30
#define DIGIT_EMPTY         0x20

#define RAW_IR_DIGIT_0 0b10111110 /* hex: 0xbe */
#define RAW_IR_DIGIT_1 0b10100000 /* hex: 0xa0 */
#define RAW_IR_DIGIT_2 0b11011010 /* hex: 0xda */
#define RAW_IR_DIGIT_3 0b11111000 /* hex: 0xf8 */
#define RAW_IR_DIGIT_4 0b11100100 /* hex: 0xe4 */
#define RAW_IR_DIGIT_5 0b01111100 /* hex: 0x7c */
#define RAW_IR_DIGIT_6 0b01111110 /* hex: 0x7e */
#define RAW_IR_DIGIT_7 0b10101000 /* hex: 0xa8 */
#define RAW_IR_DIGIT_8 0b11111110 /* hex: 0xfe */
#define RAW_IR_DIGIT_9 0b11111100 /* hex: 0xfc */
#define RAW_IR_DIGIT_L 0b00010110 /* hex: 0x16 */

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

// raw data: -0.1234 mV AC
static const uint8_t rawIRDataNegativeACVoltage[16] = {
        0b00000111, // negative, AC
        0xbe, //0
        0xa0 | 0x01, //.1
        0xda, //2
        0xf8, //3
        0xe4, //4
        0x00, //nothing
        0b10100000, //mV
        0, 0, 0, 0, 0, 0, 0, 0
};

// raw data: 12.3458 uV DC
static const uint8_t rawIRDataPositiveDCVoltage[16] = {
        0b00001001, // positive, DC
        RAW_IR_DIGIT_1, //1
        RAW_IR_DIGIT_2, //2
        RAW_IR_DIGIT_3 | 0x01, //.3
        RAW_IR_DIGIT_4, //4
        RAW_IR_DIGIT_5, //5
        RAW_IR_DIGIT_8, //8
        0b11000000, //uV
        0, 0, 0, 0, 0, 0, 0, 0
};

// raw data: 102.37 kΩ
static const uint8_t rawIRDataResistancekOhm[16] = {
        0x01, // nothing
        RAW_IR_DIGIT_1,
        RAW_IR_DIGIT_0,
        RAW_IR_DIGIT_2,
        RAW_IR_DIGIT_3 | 0x01, //.3
        RAW_IR_DIGIT_7,
        0x00,
        0x00,
        0b00000110, //kΩ
        0, 0, 0, 0, 0, 0, 0
};

// raw data: DC or AC (auto) 91.3790 nA with autorange
static const uint8_t rawIRDataNanoAmps[16] = {
        0b10001101, // autorange, AC and DC symbols
        RAW_IR_DIGIT_9,
        RAW_IR_DIGIT_1,
        RAW_IR_DIGIT_3 | 0x01, // .3
        RAW_IR_DIGIT_7,
        RAW_IR_DIGIT_9,
        RAW_IR_DIGIT_0,
        0b00001100, // nA
        0, 0, 0, 0, 0, 0, 0, 0
};

// raw data: Over limit indication
static const uint8_t rawIRDataOverLimit[16] = {
        0b00001101, //  AC and DC symbols
        0x00,
        0x00,
        RAW_IR_DIGIT_0 | 0x01, // .0
        RAW_IR_DIGIT_L,
        0x00,
        0x00,
        0b00001100, // nA
        0, 0, 0, 0, 0, 0, 0, 0
};

// forward declarations of static functions
void bm_calculate_pkt_check_sum(data_resp_pkt* const pRespPack);
void bm_fill_pkt_constants(data_resp_pkt* const pRespPack);
uint8_t convert_digit_segs_to_val(uint8_t segments);
void convert_sanwa_ir_data_to_bm_pkt(const uint8_t* const pRawData, data_resp_pkt* const pPkg);



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
} // test_bm_calculate_pkt_check_sum

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
} // test_bm_fill_pkt_constants

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
} // test_convert_digit_segs_to_val

void test_convert_sanwa_ir_data_to_bm_pkt(void) {
    data_resp_pkt packet = {0};

    // TEST CASE 1 AC voltage
    convert_sanwa_ir_data_to_bm_pkt(&rawIRDataNegativeACVoltage[0], &packet);
    // check for AC and V symbol
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_AC | BM_PROTO_SYM_V, packet.func[0]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);
    // check for minus sign
    TEST_ASSERT_BITS_HIGH(MINUS_SIGN, packet.valSign);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){
        DIGIT_0, 0 /*Constants not filled*/, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_EMPTY, 0 /*constants not filled */, MINUS_SIGN, DIGIT_3 /*E-3*/}),
        &packet.asciiAndTailLong.d1, sizeof(packet.asciiAndTailLong) - sizeof(packet.asciiAndTailLong.pktTail));

    // TEST CASE 2 DV voltage
    memset(&packet, 0, sizeof(packet));
    convert_sanwa_ir_data_to_bm_pkt(&rawIRDataPositiveDCVoltage[0], &packet);
    // check for DC and V symbol
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_DC | BM_PROTO_SYM_V, packet.func[0]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);
    // check if there is not a minus sign
    TEST_ASSERT_BITS_HIGH(DIGIT_EMPTY, packet.valSign);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){
            DIGIT_1, 0 /*Constants not filled*/, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_8, 0 /*constants not filled */, MINUS_SIGN, DIGIT_5 /*E-6 * 10*/}),
            &packet.asciiAndTailLong.d1, sizeof(packet.asciiAndTailLong) - sizeof(packet.asciiAndTailLong.pktTail));


    // TEST CASE 3: resistance kΩ
    memset(&packet, 0, sizeof(packet));
    convert_sanwa_ir_data_to_bm_pkt(&rawIRDataResistancekOhm[0], &packet);
    // check for Ω symbol
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_Ohm, packet.func[0]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);
    // check if there is not a minus sign
    TEST_ASSERT_BITS_HIGH(DIGIT_EMPTY, packet.valSign);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){
        DIGIT_1, 0 /*Constants not filled*/, DIGIT_0, DIGIT_2, DIGIT_3, DIGIT_7, DIGIT_EMPTY, 0 /*constants not filled */, PLUS_SIGN, DIGIT_5 /*E+3 * 100*/}),
        &packet.asciiAndTailLong.d1, sizeof(packet.asciiAndTailLong) - sizeof(packet.asciiAndTailLong.pktTail));

    // TEST CASE 4: nA
    memset(&packet, 0, sizeof(packet));
    convert_sanwa_ir_data_to_bm_pkt(&rawIRDataNanoAmps[0], &packet);
    // check for AC and DC and A symbol
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_DC | BM_PROTO_SYM_AC, packet.func[0]);
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_A, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);
    // check if there is not a minus sign
    TEST_ASSERT_BITS_HIGH(DIGIT_EMPTY, packet.valSign);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){
        DIGIT_9, 0 /*Constants not filled*/, DIGIT_1, DIGIT_3, DIGIT_7, DIGIT_9, DIGIT_0, 0 /*constants not filled */, MINUS_SIGN, DIGIT_8 /*E-9 * 10*/}),
        &packet.asciiAndTailLong.d1, sizeof(packet.asciiAndTailLong) - sizeof(packet.asciiAndTailLong.pktTail));

} // test_convert_sanwa_ir_data_to_bm_pkt

void test_convert_sanwa_ir_data_to_bm_pkt_OVER_LIMIT(void) {
    data_resp_pkt packet = {0};

    convert_sanwa_ir_data_to_bm_pkt(&rawIRDataOverLimit[0], &packet);
    // Check for AC, DC and A symbols
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_DC | BM_PROTO_SYM_AC, packet.func[0]);
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_A, packet.func[1]);
    // check if there is no minus sign
    TEST_ASSERT_BITS_HIGH(DIGIT_EMPTY, packet.valSign);
    // check for OL symbol
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){BM_O_CHAR /*O char*/, BM_L_CHAR /*L char*/}),
        &packet.asciiAndTailShort.oChar, sizeof(packet.asciiAndTailShort) - sizeof(packet.asciiAndTailShort.pktTail));

} // test_convert_sanwa_ir_data_to_bm_pkt_OVER_LIMIT

void test_bm_create_pkt(void) {
    data_resp_pkt packet = {0};

    bm_result result = bm_create_pkt(&rawIRDataNegativeACVoltage[0], 1, &packet);
    TEST_ASSERT_EQUAL(BM_RAW_DATA_LEN_TOO_SHORT, result);

    result = bm_create_pkt(&rawIRDataNegativeACVoltage[0], sizeof(rawIRDataNegativeACVoltage), &packet);
    TEST_ASSERT_EQUAL(BM_PKG_CREATED, result);

    // check constants inside the packet
    TEST_ASSERT_EQUAL_UINT8(0x10, packet.header.dle);
    TEST_ASSERT_EQUAL_UINT8(0x02, packet.header.stx);
    TEST_ASSERT_EQUAL_UINT8(0x00, packet.header.cmd);
    TEST_ASSERT_EQUAL_UINT8(BM_DOT_CHAR_CONST, packet.asciiAndTailLong.constDotChar);
    TEST_ASSERT_EQUAL_UINT8(0x45, packet.asciiAndTailLong.constEChar);
    TEST_ASSERT_EQUAL_UINT8(0x10, packet.asciiAndTailLong.pktTail.dle);
    TEST_ASSERT_EQUAL_UINT8(0x03, packet.asciiAndTailLong.pktTail.etx);

    TEST_ASSERT_EQUAL_UINT8(BM_NORMAL_PACKET_DATA_LENGTH, packet.header.dataLen);

    // Checking data readings
    // check for AC and DC symbols
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_AC | BM_PROTO_SYM_V, packet.func[0]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);
    // check for minus sign
    TEST_ASSERT_BITS_HIGH(MINUS_SIGN, packet.valSign);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){
        DIGIT_0, BM_DOT_CHAR_CONST, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_EMPTY, BM_EXPONENT_CHAR_CONST, MINUS_SIGN, DIGIT_3 /*E-3*/}),
        &packet.asciiAndTailLong.d1, sizeof(packet.asciiAndTailLong) - sizeof(packet.asciiAndTailLong.pktTail));

    // calculate checksum
    uint8_t checkSum = packet.func[0];
    uint8_t* pByte = &packet.func[1];
    for (int i = 1; i < BM_NORMAL_PACKET_DATA_LENGTH; ++i) {
        checkSum ^= *pByte;
        ++pByte;
    }

    TEST_ASSERT_EQUAL(checkSum, packet.asciiAndTailLong.pktTail.chkSum);
} // test_bm_create_pkt

void test_bm_create_pkt_OVER_LIMIT(void) {
    data_resp_pkt packet = {0};

    bm_result result = bm_create_pkt(&rawIRDataOverLimit[0], sizeof(rawIRDataOverLimit), &packet);
    TEST_ASSERT_EQUAL(BM_PKG_CREATED, result);

    // check constants inside the packet
    TEST_ASSERT_EQUAL_UINT8(0x10, packet.header.dle);
    TEST_ASSERT_EQUAL_UINT8(0x02, packet.header.stx);
    TEST_ASSERT_EQUAL_UINT8(0x01, packet.header.cmd);
    TEST_ASSERT_EQUAL_UINT8(0x10, packet.asciiAndTailShort.pktTail.dle);
    TEST_ASSERT_EQUAL_UINT8(0x03, packet.asciiAndTailShort.pktTail.etx);

    // check for OL
    TEST_ASSERT_EQUAL_UINT8(DIGIT_EMPTY, packet.valSign);
    TEST_ASSERT_EQUAL_UINT8(DIGIT_O, packet.asciiAndTailShort.oChar);
    TEST_ASSERT_EQUAL_UINT8(DIGIT_L, packet.asciiAndTailShort.lChar);

    // check length
    TEST_ASSERT_EQUAL_UINT8(BM_OL_PACKET_DATA_LENGTH, packet.header.dataLen);

    // check for AC and DC symbols
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_AC | BM_PROTO_SYM_DC, packet.func[0]);
    TEST_ASSERT_BITS(0xFF, BM_PROTO_SYM_A, packet.func[1]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[2]);
    TEST_ASSERT_BITS_LOW(0xFF, packet.func[3]);

    // check checksum
    uint8_t checkSum = packet.func[0];
    uint8_t* pByte = &packet.func[1];
    for (int i = 1; i < BM_OL_PACKET_DATA_LENGTH; ++i) {
        checkSum ^= *pByte;
        ++pByte;
    }
    TEST_ASSERT_EQUAL_UINT8(checkSum, packet.asciiAndTailShort.pktTail.chkSum);
} // test_bm_create_pkt_OVER_LIMIT



int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bm_calculate_pkt_check_sum);
    RUN_TEST(test_bm_fill_pkt_constants);
    RUN_TEST(test_convert_digit_segs_to_val);
    RUN_TEST(test_convert_sanwa_ir_data_to_bm_pkt);
    RUN_TEST(test_convert_sanwa_ir_data_to_bm_pkt_OVER_LIMIT);
    RUN_TEST(test_bm_create_pkt);
    RUN_TEST(test_bm_create_pkt_OVER_LIMIT);
    return UNITY_END();
}
