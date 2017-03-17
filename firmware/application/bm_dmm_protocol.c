#include "bm_dmm_protocol.h"
#include "bm_protocol_bits_descr.h"

#include <stdint.h>
#include <stddef.h>
#include <bool.h>

/// Constants used in protocol packets
#define BM_DLE_CONST 0x10
#define BM_STX_CONST 0x02
#define BM_ETX_CONST 0x03
#define BM_PKT_DATA_LENGHT 0x0F
/// Supported commands:
#define BM_DATA_REQ_COMMAND 0x00

/// Sanwa PC5000A IR protocol length in bytes
#define SANWA_DATA_LEN 16

/// Except of digits in range of 0-9 on display can be seen also: L (on OL measuring)) or digits can be 'empty'
#define DIGIT_INVALID_VALUE (UINTN_MAX(8))
#define DIGIT_EMPTY         0x20
#define DIGIT_L             0x4C
#define DIGIT_O             0x4F
#define DIGIT_DASH          0x2D
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

#define RAW_BIT(data, bit) ((data) & (1 << (bit)))


static void bm_fill_pkt_constants(data_resp_pkg* const pRespPack);
static void bm_calculate_pkt_check_sum(data_resp_pkg* const pRespPack);
static void convert_sanwa_ir_data_to_bm_pkt(uint8_t* const pRawData, data_resp_pkg* const pPkg);
static uint8_t convert_digit_segs_to_val(uint8_t segments);

bm_result bm_create_pkt(uint8_t* const pRawData, const uint8_t rawDataLen, data_resp_pkg* const pDestPkg) {
    bm_result retVal = BM_ERROR;

    if (NULL != pRawData && NULL != pDestPkg) {
        if (rawDataLen >= SANWA_DATA_LEN) {
            // clear package memory - only func fields need to be cleared
            memset((void*)pDestPkg->func, 0, sizeof(pDestPkg->func));
            // convert from raw data:
            convert_sanwa_ir_data_to_bm_pkt(pRawData, pDestPkg);
            // calculate the check sum
            bm_calculate_pkt_check_sum(pDestPkg);
            // fill constant values in the package
            bm_fill_pkt_constants(pDestPkg);
            retVal = BM_PKG_CREATED;
        } else {
            retVal = BM_RAW_DATA_LEN_TOO_SHORT;
        }
    }
    return retVal;
}


static inline void bm_calculate_pkt_check_sum(data_resp_pkg* const pRespPack) {
    if (NULL != pRespPack) {
        // check sum is calculated by XOR bytes from FUNCs, and ASCII reading
        uint8_t chkSum = pRespPack->func[0];

        uint8_t* pBytes = &pRespPack->func[1];
        for (int i = 1; i < pRespPack->header.dataLen - 1; ++i) {
            chkSum ^= *pBytes;
            ++pBytes;
        }

        // apply check sum to the packet
        if (BM_PKT_DATA_LENGHT == pRespPack->header.dataLen) {
            pRespPack->asciiAndTailLong.pktTail.chkSum = chkSum;
        } else {
            pRespPack->asciiAndTailShort.pktTail.chkSum = chkSum;
        }
    }
}


static inline void bm_fill_pkt_constants(data_resp_pkg* const pRespPack, const uint8_t cmd) {
    if (NULL != pRespPack) {
        pRespPack->header.dle = BM_DLE_CONST;
        pRespPack->header.stx = BM_STX_CONST;
        pRespPack->header.cmd = cmd;

        if (BM_PKT_DATA_LENGHT == pRespPack->header.dataLen) {
            pRespPack->asciiAndTailLong.pktTail.etx = BM_ETX_CONST;
            pRespPack->asciiAndTailLong.pktTail.dle = BM_DLE_CONST;
        } else {
            pRespPack->asciiAndTailShort.pktTail.etx = BM_ETX_CONST;
            pRespPack->asciiAndTailShort.pktTail.dle = BM_DLE_CONST;
        }
    }
}


static void convert_sanwa_ir_data_to_bm_pkt(uint8_t* const pRawData, data_resp_pkg* const pPkg) {
    uint8_t chByte;
    uint8_t digit = DIGIT_EMPTY;

//    TEST BYTE 0
//    Byte 0
//    BIT MEANING
//    0   Start IR frame
//    1   Minus before digits (negative value)
//    2   AC symbol
//    3   DC symbol
//    4   Rel (relative measurement)
//    5   Low battery?
//    6   Hold symbol
//    7   Auto symbol (auto range)
    chByte = pRawData[0];
    // Test for minus sign (negative value)
    (0 == RAW_BIT(chByte,1)) ? pPkg->valSign = 0x20 : pPkg->valSign = 0x2D;

    // Test for AC symbol
    if (0 != RAW_BIT(chByte, 2)) {
        pPkg->func[0] |= BM_PROTO_SYM_AC;
    }

    // Test for DC symbol
    if (0 != RAW_BIT(chByte, 3)) {
        pPkg->func[0] |= BM_PROTO_SYM_DC;
    }

    // Test for Low battery symbol
    if (0 != RAW_BIT(chByte, 5)) {
        pPkg->func[3] |= BM_PROTO_SYM_LOWBAT;
    }



//    TEST BYTE 1
//    Byte 1
//    BIT meaning
//    0   beep symbol
//    1   E-segment of 1st digit
//    2   F-segment of 1st digit
//    3   A-segment of 1st digit
//    4   D-segment of 1st digit
//    5   C-segment of 1st digit
//    6   G-segment of 1st digit
//    7   B-segment of 1st digit
    chByte = pRawData[1];
    // Test for beep symbol
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->func[1] |= BM_PROTO_SYM_BEEP;
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d1 = digit) : (pPkg->asciiAndTailLong.d1 = DIGIT_EMPTY) ;


    //    TEST BYTE 2
    //    Byte 2
    //    BIT meaning
    //    0   dot after first digit
    //    1   E-segment of 2nd digit
    //    2   F-segment of 2nd digit
    //    3   A-segment of 2nd digit
    //    4   D-segment of 2nd digit
    //    5   C-segment of 2nd digit
    //    6   G-segment of 2nd digit
    //    7   B-segment of 2nd digit

    chByte = pRawData[2];
    // save dot place if set
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->asciiAndTailLong.exponent = 0; // dot after first digit in protocol means: val x 1
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d2 = digit) : (pPkg->asciiAndTailLong.d2 = DIGIT_EMPTY) ;



    //    TEST BYTE 3
    //    Byte 3
    //    BIT meaning
    //    0   dot after second digit
    //    1   E-segment of 3rd digit
    //    2   F-segment of 3rd digit
    //    3   A-segment of 3rd digit
    //    4   D-segment of 3rd digit
    //    5   C-segment of 3rd digit
    //    6   G-segment of 3rd digit
    //    7   B-segment of 3rd digit

    chByte = pRawData[3];
    // save dot place if set
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->asciiAndTailLong.exponent = 1; // dot after second digit in protocol means: val x 10^1
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d3 = digit) : (pPkg->asciiAndTailLong.d3 = DIGIT_EMPTY) ;



    //    TEST BYTE 4
    //    Byte 4
    //    BIT meaning
    //    0   dot after third digit
    //    1   E-segment of 4th digit
    //    2   F-segment of 4th digit
    //    3   A-segment of 4th digit
    //    4   D-segment of 4th digit
    //    5   C-segment of 4th digit
    //    6   G-segment of 4th digit
    //    7   B-segment of 4th digit

    chByte = pRawData[4];
    // save dot place if set
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->asciiAndTailLong.exponent = 2; // dot after third digit in protocol means: val x 10^2
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d4 = digit) : (pPkg->asciiAndTailLong.d4 = DIGIT_EMPTY) ;



    //    TEST BYTE 5
    //    Byte 5
    //    BIT meaning
    //    0   dot after fourth digit
    //    1   E-segment of 5th digit
    //    2   F-segment of 5th digit
    //    3   A-segment of 5th digit
    //    4   D-segment of 5th digit
    //    5   C-segment of 5th digit
    //    6   G-segment of 5th digit
    //    7   B-segment of 5th digit
    chByte = pRawData[5];
    // save dot place if set
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->asciiAndTailLong.exponent = 3; // dot after fourth digit in protocol means: val x 10^3
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d5 = digit) : (pPkg->asciiAndTailLong.d5 = DIGIT_EMPTY) ;


    //    TEST BYTE 6
    //    Byte 6
    //    BIT meaning
    //    0   dot after fifth digit?
    //    1   E-segment of 6th digit
    //    2   F-segment of 6th digit
    //    3   A-segment of 6th digit
    //    4   D-segment of 6th digit
    //    5   C-segment of 6th digit
    //    6   G-segment of 6th digit
    //    7   B-segment of 6th digit

    chByte = pRawData[6];
    // save dot place if it's set
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->asciiAndTailLong.exponent = 4; // dot after fifth digit in protocol means: val x 10^4
    }
    // Get digit
    digit = convert_digit_segs_to_val(chByte);
    (DIGIT_INVALID_VALUE != digit) ? (pPkg->asciiAndTailLong.d6 = digit) : (pPkg->asciiAndTailLong.d6 = DIGIT_EMPTY) ;


    //    TEST BYTE 7
    //    Byte 7
    //    BIT meaning
    //    0
    //    1   F
    //    2   n (1e-9)
    //    3   A
    //    4   dB
    //    5   m (1e-3)
    //    6   u (1e-6)
    //    7   V
    chByte = pRawData[7];
    // test for F symbol (capacitance)
    if (0 != RAW_BIT(chByte, 1)) {
        pPkg->func[0] |= BM_PROTO_SYM_Cx;
    }
    // test for n symbol
    if (0 != RAW_BIT(chByte, 2)) {
        // in this protocol need to change the exponent. Exponent already has been set when parsing digits,
        // so need to take that into account
        pPkg->asciiAndTailLong->exponent = 9 - pPkg->asciiAndTailLong->exponent; // nano's exponent is 9
    }
    // test for A symbol
    if (0 != RAW_BIT(chByte, 1)) {
        pPkg->func[1] |= BM_PROTO_SYM_A;
    }
    // test for dB symbol
    if (0 != RAW_BIT(chByte, 1)) {
        pPkg->func[1] |= BM_PROTO_SYM_dB;
    }
    // test for m symbol
    if (0 != RAW_BIT(chByte, 5)) {
        // in this protocol need to change the exponent. Exponent already has been set when parsing digits,
        // so need to take that into account
        // todo does DMM provide combination that this will overfill?
        pPkg->asciiAndTailLong->exponent = 3 - pPkg->asciiAndTailLong->exponent; // milli's exponent is 3
    }
    // test for u symbol
    if (0 != RAW_BIT(chByte, 6)) {
        // in this protocol need to change the exponent. Exponent already has been set when parsing digits,
        // so need to take that into account
        pPkg->asciiAndTailLong->exponent = 6 - pPkg->asciiAndTailLong->exponent; // micro's exponent is 6
    }
    // test for V symbol
    if (0 != RAW_BIT(chByte, 1)) {
        pPkg->func[0] |= BM_PROTO_SYM_V;
    }


    //    TEST BYTE 8
    //    Byte 8
    //    BIT meaning
    //    0   Hz
    //    1   Ohm
    //    2   k
    //    3   M
    //    4   ???
    //    5   MIN
    //    6   ???
    //    7   % (duty)
    chByte = pRawData[8];
    // test for Hz symbol
    if (0 != RAW_BIT(chByte, 0)) {
        pPkg->func[1] |= BM_PROTO_SYM_Hz;
    }
    // test for Ohm symbol
    if (0 != RAW_BIT(chByte, 1)) {
        pPkg->func[0] |= BM_PROTO_SYM_Ohm;
    }
    // test for k symbol
    if (0 != RAW_BIT(chByte, 2)) {
        // in this protocol need to change the exponent. Exponent already has been set when parsing digits,
        // so need to take that into account
        pPkg->asciiAndTailLong->exponent += 3; // kilo's exponent is 3
    }
    // test for M symbol
    if (0 != RAW_BIT(chByte, 3)) {
        // in this protocol need to change the exponent. Exponent already has been set when parsing digits,
        // so need to take that into account
        pPkg->asciiAndTailLong->exponent += 6; // mega's exponent is 6
    }

    // skipping bit: 5,6 and 4 MIN is not supported by this protocol

    // test for % symbol
    if (0 != RAW_BIT(chByte, 7)) {
        pPkg->func[1] |= BM_PROTO_SYM_PERCENTAGE;
    }


    //    Byte 9
    //    BIT meaning
    //    0   dash between MIN and MAX symbols
    //    1   ???
    //    2   MAX symbol
    //    3   R symbol - record mode
    //    4   ???
    //    5   ???
    //    6   ???
    //    7   C symbol - capture mode


    //    BYTE 10
    //    BIT meaning
    //    0   ???
    //    1   ???
    //    2   ???
    //    3   ???
    //    4   bar graph: 39
    //    5   bar graph: 40
    //    6   bar graph: 41
    //    7   arrow at the end of bar graph
    //
    //    BYTE 11
    //    BIT meaning
    //    0   bar graph: 38
    //    1   bar graph: 37
    //    2   bar graph: 36
    //    3   bar graph: 35
    //    4   bar graph: 31
    //    5   bar graph: 32
    //    6   bar graph: 33
    //    7   bar graph: 34

    //    BYTE 12
    //    BIT meaning
    //    0   bar graph: 30
    //    1   bar graph: 29
    //    2   bar graph: 28
    //    3   bar graph: 27
    //    4   bar graph: 23
    //    5   bar graph: 24
    //    6   bar graph: 25
    //    7   bar graph: 26

    //    BYTE 13
    //    BIT meaning
    //    0   bar graph: 22
    //    1   bar graph: 21
    //    2   bar graph: 20
    //    3   bar graph: 19
    //    4   bar graph: 15
    //    5   bar graph: 16
    //    6   bar graph: 17
    //    7   bar graph: 18

    //    BYTE 14
    //    BIT meaning
    //    0   bar graph: 14
    //    1   bar graph: 13
    //    2   bar graph: 12
    //    3   bar graph: 11
    //    4   bar graph: 7
    //    5   bar graph: 8
    //    6   bar graph: 9
    //    7   bar graph: 10

    //    BYTE 15
    //    BIT meaning
    //    0   bar graph: 6
    //    1   bar graph: 5
    //    2   bar graph: 4
    //    3   bar graph: 3
    //    4   bar graph: scale
    //    5   bar graph: scale
    //    6   bar graph: 1
    //    7   bar graph: 2
}



static uint8_t convert_digit_segs_to_val(uint8_t segments) {
    //    BIT meaning
    //    0   must be set to 0
    //    1   E-segment of 1st digit
    //    2   F-segment of 1st digit
    //    3   A-segment of 1st digit
    //    4   D-segment of 1st digit
    //    5   C-segment of 1st digit
    //    6   G-segment of 1st digit
    //    7   B-segment of 1st digit
    // -----------------------------------
    // 1 segs: b,c -> 160
    // 2 segs: a,b,g,e,d -> 218
    // 3 segs: a,b,g,c,d -> 248
    // 4 segs: f,g,c,b -> 228
    // 5 segs: a,f,g,c,d -> 124
    // 6 segs: a,f,e,d,c,g -> 126
    // 7 segs: a,b,c -> 168
    // 8 segs: a,b,c,d,e,f,g -> 254
    // 9 segs: a,b,c,d,g,f -> 252
    // 0 segs: a,b,c,d,e,f -> 190
    // L segs: f,e,d -> 22
    typedef enum  {
        D1 = 160,
        D2 = 218,
        D3 = 248,
        D4 = 228,
        D5 = 124,
        D6 = 126,
        D7 = 168,
        D8 = 254,
        D9 = 252,
        D0 = 190,
        DEMPTY = 0,
        DL = 22
    } DIGITS;

    uint8_t retVal;
    segments &= ~(1 << 0);

    switch ((DIGITS)segments) {
    case D1:     retVal = DIGIT_1;     break;
    case D2:     retVal = DIGIT_2;     break;
    case D3:     retVal = DIGIT_3;     break;
    case D4:     retVal = DIGIT_4;     break;
    case D5:     retVal = DIGIT_5;     break;
    case D6:     retVal = DIGIT_6;     break;
    case D7:     retVal = DIGIT_7;     break;
    case D8:     retVal = DIGIT_8;     break;
    case D9:     retVal = DIGIT_9;     break;
    case D0:     retVal = DIGIT_0;     break;
    case DEMPTY: retVal = DIGIT_EMPTY; break;
    case DL:     retVal = DIGIT_L;     break;
    default:     retVal = DIGIT_INVALID_VALUE; break;
    };

    return retVal;
}
