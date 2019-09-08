#include "minunit.h"
#include "associative_array.c"
#include "random_string.c"

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


static char * test_elem_heights_on_insertion() {
    int depth_level=0;
    aa = aa_create();
    
    aa_put(aa, "first", "1");
    mu_assert("The height of the root element should be 1", aa->root->height==1);
    depth_level = aa_has(aa->root, "first");
    mu_assert("The element 'first' should be at depth 1.", depth_level==1);
  
    // add second element
    aa_put(aa, "second", "2");
    mu_assert("The weight of the root element should be +1", aa->root->height==2);
    depth_level = aa_has(aa->root, "second");
    mu_assert("The element 'second' should be at depth 2.", depth_level==2);

    aa_free(aa);
    return 0;
}

static char * test_balance_with_rotate_left() {    
    // GIVEN
    aa = aa_create();
    int depth_level=0;
    
    aa_put(aa, "first", "1");
    aa_put(aa, "second", "2");
    aa_put(aa, "third", "3");
    
    // Should trigger rotating in the following way:
    // From:
    // first(root)-> second -> third
    // To:
    // first <- second(root) -> third
    
    mu_assert("The tree should balance itself.", aa->root->height==2);
    int eq = strcmp(aa->root->key, "second");
    mu_assert("The new root should be 'second'", eq==0);
    
    aa_free(aa);
    return 0;
}

static char * test_balance_with_rotate_right() {    
    // GIVEN
    aa = aa_create();
    int depth_level=0;
    
    aa_put(aa, "third", "3");
    aa_put(aa, "second", "2");
    aa_put(aa, "first", "1");
    
    // Should trigger rotating in the following way:
    // From:
    // first <- second <- third(root)
    // To:
    // first <- second(root) -> third
    
    mu_assert("The tree should balance itself.", aa->root->height==2);
    int eq = strcmp(aa->root->key, "second");
    mu_assert("The new root should be 'second'", eq==0);
    
    aa_free(aa);
    return 0;
}

/**
 * left_right rotate: 
 * rotate left in the deeper level, then rotate right on the grandparent level
 * @return 
 */
static char * test_balance_with_rotate_left_right() {    
    // GIVEN
    aa = aa_create();
    int depth_level=0;
    
    aa_put(aa, "third", "3");
    aa_put(aa, "second", "2");
    aa_put(aa, "sfcond", "1");
    
    // Should trigger rotating in the following way:
    // From:
    // sfcond <- second <- third(root)
    // To:
    // second <- sfcond(root) -> third
    
    mu_assert("The tree should balance itself.", aa->root->height==2);
    int eq = strcmp(aa->root->key, "sfcond");
    mu_assert("The new root should be 'sfcond'", eq==0);
    
    eq=strcmp(aa->root->right->key, "third");
    mu_assert("The new root->right should be 'third'", eq==0);
    
    aa_free(aa);
    return 0;
}

static char * test_balance_with_rotate_right_left() {    
    // GIVEN
    aa = aa_create();
    int depth_level=0;
    
    aa_put(aa, "first", "1");
    aa_put(aa, "second", "2");
    aa_put(aa, "sbcond", "3");
    
    mu_assert("The tree should balance itself.", aa->root->height==2);
    int eq = strcmp(aa->root->key, "sbcond");
    mu_assert("The new root should be 'sbcond'", eq==0);
    
    eq=strcmp(aa->root->right->key, "second");
    mu_assert("The new root->right should be 'second'", eq==0);
    
    aa_free(aa);
    return 0;
}

static char * test_balance_with_a_load() {
    //insert 1024 random string
    //total height must remain 10;
    const int count=1024;
    char* strings[count];
    for(int i=0;i<count;i++) strings[i] = random_str(20);
    
    aa = aa_create();
    
    for(int i=0;i<count;i++) aa_put(aa, strings[i], "1");
    
    mu_assert("The tree with 1024 strings should be 12 tall tops",
            aa->root->height<=12);
    
    //printf("%s %d\n",aa->root->key,aa->root->height);
    
    for(int i=0;i<count;i++) free(strings[i]);
    
    aa_free(aa);
    return 0;
}

static char * test_associative_array() {
    mu_run_test(test_get_returns_false_on_empty_arr);
    mu_run_test(test_should_find_values);
    mu_run_test(test_elem_heights_on_insertion);
    mu_run_test(test_balance_with_rotate_left);
    mu_run_test(test_balance_with_rotate_right);
    mu_run_test(test_balance_with_rotate_left_right);
    mu_run_test(test_balance_with_rotate_right_left);
    mu_run_test(test_balance_with_a_load);

    return 0;
}