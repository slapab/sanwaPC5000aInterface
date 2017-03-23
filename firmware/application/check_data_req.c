#include <stddef.h>
#include "check_data_req.h"
#include "bm_protocol_defs.h"

/**
 *  Checks for valid request for data. Stores information about previous checking, that is when request come in parts
 *  it will be able to match request if bytes will represent an valid request.
 *
 *  @param buff pointer to the buffer where request bytes are storing
 *  @param size number of bytes inside buffer at given time
 *  @return number of matched requests in this function call. It will return value greater that number of full request
 *  stored inside buffer if previous call was not complete and in current call there was missing request bytes.
 */
uint8_t check_buffer_for_data_request(const uint8_t* const buff, const size_t size) {
    enum {STATES_NUM = 8};
    static const uint8_t statesTab[STATES_NUM] = {BM_DLE_CONST, BM_STX_CONST, BM_DATA_REQ_COMMAND, 0, 0, 0, BM_DLE_CONST, BM_ETX_CONST};
    static uint8_t currState = 0;

    uint8_t retval = 0;

    if ((NULL != buff) && (size > 0)) {
        for (size_t i = 0; i < size; ++i) {
            if (statesTab[currState] == buff[i]) {
                ++currState;
                if (currState >= STATES_NUM) {
                    currState = 0;
                    ++retval;
                }
            } else {
                currState = 0;
            }
        }
    }
    return retval;
}
