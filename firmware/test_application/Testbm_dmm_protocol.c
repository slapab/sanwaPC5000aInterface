#include "bm_dmm_protocol.h"
#include "unity.h"

// forward declarations of static functions
int exampleFunc(void);

void test_exampleFunc_return(void) {

     TEST_ASSERT_EQUAL(0, exampleFunc());

}


int main(void) {

    UNITY_BEGIN();
    RUN_TEST(test_exampleFunc_return);
    return UNITY_END();

}
