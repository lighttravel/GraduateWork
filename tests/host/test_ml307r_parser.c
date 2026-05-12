#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define ML307R_DIAG_TEST
#include "../../main/services/ml307r/ml307r_service.c"

static void test_parse_csq_plain_response(void)
{
    int rssi = 99;
    int ber = 99;

    assert(parse_csq("\r\n+CSQ: 18,3\r\n\r\nOK\r\n", &rssi, &ber));
    assert(rssi == 18);
    assert(ber == 3);
}

static void test_parse_csq_quoted_response(void)
{
    int rssi = 99;
    int ber = 99;

    assert(parse_csq("\r\n+CSQ: \"99\",\"23\"\r\n\r\nOK\r\n", &rssi, &ber));
    assert(rssi == 99);
    assert(ber == 23);
}

static void test_parse_registration_stat_standard_response(void)
{
    int stat = -1;

    assert(parse_registration_stat("\r\n+CEREG: 2,5,\"1234\",\"5678\",7\r\n\r\nOK\r\n", "+CEREG", &stat));
    assert(stat == 5);
    assert(registration_stat_is_registered(stat));
}

static void test_parse_registration_stat_ml307r_quoted_response(void)
{
    int stat = -1;

    assert(parse_registration_stat("\r\n+CEREG=\"1\"\r\n\r\nOK\r\n", "+CEREG", &stat));
    assert(stat == 1);
    assert(registration_stat_is_registered(stat));
}

static void test_response_status_helpers(void)
{
    assert(response_has_ok("\r\nOK\r\n"));
    assert(response_has_error("\r\n+CME ERROR: 10\r\n"));
    assert(!response_has_ok("\r\n+CPIN: READY\r\n"));
}

int main(void)
{
    test_parse_csq_plain_response();
    test_parse_csq_quoted_response();
    test_parse_registration_stat_standard_response();
    test_parse_registration_stat_ml307r_quoted_response();
    test_response_status_helpers();
    puts("ml307r parser tests passed");
    return 0;
}
