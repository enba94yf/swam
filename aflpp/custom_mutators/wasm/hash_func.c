#include <stdint.h>

uint64_t djb2_hash(unsigned char* str, int len) {

    uint64_t hash = 5381;
    int c = 0;

    while (c < len) {
        hash = ((hash << 5) + hash) + str[c];
        c++;
    }

    return hash;

}