#include "minunit.h"
#include "associative_array.c"

associative_array *aa;

static char * test_get_returns_false_on_empty_arr() {    
    // GIVEN
    aa = aa_create();
    // WHEN
    aa_item* has = aa_get(aa->root, "example");
    // THEN
    mu_assert("Get expected to return falsy.", !has);
    // WHEN
    int exists = aa_has(aa->root, "example");
    // THEN
    mu_assert("Has expected to return falsy.", !exists);

    aa_free(aa);
    return 0;
}

static char * test_should_find_values() {    
    // GIVEN
    aa = aa_create();
    
    aa_put(aa, "first", "1");
    aa_put(aa, "second", "2");
    
    // WHEN
    aa_item* has = aa_get(aa->root, "first");
    // THEN
    mu_assert("Get expected to find 'first' as match.", has);
    // WHEN
    int exists = aa_has(aa->root, "second");
    // THEN
    mu_assert("Has expected to find 'second' as an existing item", exists);
    
    aa_free(aa);
    return 0;
}


static char * test_associative_array() {

    mu_run_test(test_get_returns_false_on_empty_arr);
    
    mu_run_test(test_should_find_values);

    return 0;
}