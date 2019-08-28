/* file: minunit.h */
#ifndef MINUNIT_H
#define MINUNIT_H

#ifdef __cplusplus
extern "C" {
#endif

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)
int tests_run;

// matchers

int is_numeric(char c) {
    return c >= '0' && c <= '9';
}

/**
 * s1 can contain numeric characters but will get skipped at comparison
 * @param s1
 * @param s2
 * @return 0 if equals - omitting the numerals in s1
 */
int strcmp_wo_nums(char* s1,char* s2) {
    if(!s1 || !s2) return -1;
    
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if(len1<len2) return -1;
    
    for(int i=0,j=0;j<len2;){
        if(is_numeric(s1[i])) {
            i++;
            continue;
        };
        if(s1[i]!=s2[j]) return s1[i]-s2[i];
        i++;
        j++;
    }
    
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MINUNIT_H */

