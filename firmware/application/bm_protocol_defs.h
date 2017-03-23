#ifndef BM_PROTOCOL_DEFS_H_
#define BM_PROTOCOL_DEFS_H_

/// Frame constants
#define BM_DLE_CONST 0x10
#define BM_STX_CONST 0x02
#define BM_ETX_CONST 0x03


/// Supported commands:
#define BM_DATA_REQ_COMMAND 0x00
#define BM_DATA_RESP_COMMAND BM_DATA_REQ_COMMAND // value of 'command' field when sending measurements
#define BM_DATA_RESP_OV_COMMAND 0x01             // value of 'command' field when sending OverLimit packet


/// Bits description inside frame's 'func' bytes
#define BM_PROTO_SYM_AC (1 << 0)
#define BM_PROTO_SYM_DC (1 << 1)
#define BM_PROTO_SYM_V  (1 << 2)
#define BM_PROTO_SYM_Cx (1 << 3)
#define BM_PROTO_SYM_Dx (1 << 4)
#define BM_PROTO_SYM_TC (1 << 5)
#define BM_PROTO_SYM_TF (1 << 6)
#define BM_PROTO_SYM_Ohm (1 << 7)

#define BM_PROTO_SYM_BEEP (1 << 0)
#define BM_PROTO_SYM_A    (1 << 1)
#define BM_PROTO_SYM_Hz   (1 << 2)
#define BM_PROTO_SYM_PERCENTAGE (1 << 3)
#define BM_PROTO_SYM_dB         (1 << 5)

#define BM_PROTO_SYM_LOWBAT (1 << 7)

#endif // BM_PROTOCOL_DEFS_H_
