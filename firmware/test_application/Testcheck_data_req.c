#include "unity.h"
#include "check_data_req.h"
#include <stdint.h>
#include <stddef.h>
#include "bm_protocol_defs.h"


static const uint8_t validReq[8] = {BM_DLE_CONST, BM_STX_CONST, BM_DATA_REQ_COMMAND, 0, 0, 0, BM_DLE_CONST, BM_ETX_CONST};
static const uint8_t invalidReqAt4[8] = {BM_DLE_CONST, BM_STX_CONST, BM_DATA_REQ_COMMAND, 0, 1, 0, BM_DLE_CONST, BM_ETX_CONST};
static const uint8_t invalidReqAt3[8] = {BM_DLE_CONST, BM_STX_CONST, BM_DATA_REQ_COMMAND+3, 0, 0, 0, BM_DLE_CONST, BM_ETX_CONST};


void test_for_invalid_request(void) {
    uint8_t retval = check_buffer_for_data_request(invalidReqAt4, sizeof(invalidReqAt4));
    TEST_ASSERT_EQUAL(0, retval);
}

void test_for_valid_request(void) {
    uint8_t retval = check_buffer_for_data_request(validReq, sizeof(validReq));
    TEST_ASSERT_EQUAL(1, retval);
}

void test_for_valid_request_in_2_parts(void) {
    uint8_t retval = check_buffer_for_data_request(validReq, 5);
    TEST_ASSERT_EQUAL(0, retval);

    retval = check_buffer_for_data_request(&validReq[5], sizeof(validReq) - 5);
    TEST_ASSERT_EQUAL(1, retval);
}

void test_for_invalid_part_of_req_and_valid_req(void) {
    uint8_t retval = check_buffer_for_data_request(invalidReqAt3, 3);
    TEST_ASSERT_EQUAL(0, retval);

    retval = check_buffer_for_data_request(&validReq[3], sizeof(validReq) - 3);
    TEST_ASSERT_EQUAL(0, retval);

    retval = check_buffer_for_data_request(&validReq[0], sizeof(validReq));
    TEST_ASSERT_EQUAL(1, retval);
}


int main (void) {
    UNITY_BEGIN();
    RUN_TEST(test_for_invalid_request);
    RUN_TEST(test_for_valid_request);
    RUN_TEST(test_for_valid_request_in_2_parts);
    RUN_TEST(test_for_invalid_part_of_req_and_valid_req);
    return UNITY_END();
}
