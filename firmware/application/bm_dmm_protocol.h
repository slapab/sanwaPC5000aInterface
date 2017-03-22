#ifndef BM_DMM_PROTOCOL_H_
#define BM_DMM_PROTOCOL_H_

#include <stdint.h>

/// Data length inside packet which stores actual reading
#define BM_NORMAL_PACKET_DATA_LENGTH 15
/// Data length inside packet which stores Over Limit indication
#define BM_OL_PACKET_DATA_LENGTH 7

typedef struct {
    uint8_t dleS;
    uint8_t stx;
    uint8_t cmd;
    uint8_t arg[2];
    uint8_t chkSum;
    uint8_t dleE;
    uint8_t etx;
} data_req_command;

typedef struct {
    uint8_t dle;
    uint8_t stx;
    uint8_t cmd;
    uint8_t dataLen;
} data_resp_header;

typedef struct {
    uint8_t chkSum;
    uint8_t dle;
    uint8_t etx;
} data_resp_tail;

typedef struct {
    uint8_t d1;
    uint8_t constDotChar;   // always '.', 0x2E
    uint8_t d2;
    uint8_t d3;
    uint8_t d4;
    uint8_t d5;
    uint8_t d6;
    uint8_t constEChar;     // always 'E', 0x45
    uint8_t exponentSign;
    uint8_t exponent;
    data_resp_tail pktTail;
} data_resp_ascii_tail_long;

typedef struct {
    uint8_t oChar;
    uint8_t lChar;
    data_resp_tail pktTail;
} data_resp_ascii_tail_short;

typedef struct {
    data_resp_header        header;
    uint8_t                 func[4];
    uint8_t valSign;
    union {
        data_resp_ascii_tail_long asciiAndTailLong;
        data_resp_ascii_tail_short asciiAndTailShort;
    };
} data_resp_pkt;

typedef enum {
    BM_PKG_CREATED = 0,
    BM_RAW_DATA_LEN_TOO_SHORT,
    BM_ERROR
} bm_result;
/**
 * Converts raw data to UART package and returns pointer to converted
 */
bm_result bm_create_pkt(const uint8_t* const pRawData, const uint8_t rawDataLen, data_resp_pkt* const pDestPkg);

#endif // BM_DMM_PROTOCOL_H_
