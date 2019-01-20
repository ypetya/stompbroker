#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>


// conclusion of this test:
// you can not shift with more than 32bits in one go!
void main() {

    uint64_t a=1U,b=1U;

    for(int i=0;i<64;i++) {
        if(a!= (1U << i) ) {
            printf("a << %d\n",i); 
            exit(1);
        }
        a = a << 1;
        printf("%llu\n",a);
        b *= 2;
       
        if(a!=b) {
            printf("a!=b at %d\n",i);
            exit(1);
        }
        if(a&b == 0) {
            printf("a&b == 0! at %d\n",i);
            exit(1);
        }
        if((a|0) != (b|0)) {
            printf("| %d\n",i);
            exit(1);
        }
        if( (a^0) != (b^0)) {
            printf("^ %d\n",i);
            exit(1);
        }
    }
}