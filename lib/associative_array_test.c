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

static char * test_should_be_able_to_query_the_depth_of_an_item() {    
    // GIVEN
    aa = aa_create();
    int depth_level=0;
    
    aa_put(aa, "first", "1");
    mu_assert("The weight of the root element should be 0", aa->root->height==0);
    // WHEN
    depth_level = aa_has(aa->root, "first");
    // THEN
    mu_assert("The element 'first' should be at depth 1.", depth_level==1);
  
    aa_put(aa, "second", "2");
    mu_assert("The weight of the root element should be +1", aa->root->height==1);
    // WHEN
    depth_level = aa_has(aa->root, "second");
    // THEN
    mu_assert("The element 'second' should be at depth 2.", depth_level==2);

    aa_put(aa, "third", "3");
    mu_assert("The tree should balance itself.", aa->root->height==0);
    aa_put(aa, "fourth", "3");
     mu_assert("The weight of the root element should be -1", aa->root->height==-1);
   
     // Check the new balanced state
    
    // WHEN
    depth_level = aa_has(aa->root, "first");
    // THEN
    mu_assert("The element 'first' should be at depth 2.", depth_level==2);
    // WHEN
    depth_level = aa_has(aa->root, "second");
    // THEN
    mu_assert("The element 'second' should be at depth 1.", depth_level==1);
    // WHEN
    depth_level = aa_has(aa->root, "third");
    // THEN
    mu_assert("The element 'third' should be at depth 2.", depth_level==2);
    // WHEN
    depth_level = aa_has(aa->root, "fourth");
    // THEN
    mu_assert("The element 'fourth' should be at depth 2.", depth_level==2);
    
    aa_free(aa);
    return 0;
}


static char * test_associative_array() {

    mu_run_test(test_get_returns_false_on_empty_arr);
    mu_run_test(test_should_find_values);
    mu_run_test(test_should_be_able_to_query_the_depth_of_an_item);

    return 0;
}