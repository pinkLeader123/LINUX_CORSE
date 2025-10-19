#include "otp.h"
#include "sha1.h"
#include <string.h>

static uint32_t truncateToDigits(uint32_t a, int digits) {
		uint32_t p[] = {0,10,100,1000,10000,10000,100000,1000000,10000000,10000000}; 
    uint32_t res = a % p[digits];
    return res;
}

//uint8_t* hmacsha(unsigned char* key, int klen, uint64_t interval) {
//    return (uint8_t*)HMAC(EVP_sha1(), key, klen, (const unsigned char*)&interval, sizeof(interval), NULL, 0);
//	
//}

unsigned char* hmacsha(const unsigned char* key, int klen, uint64_t interval) {
    // B? d?m tinh cho digest (20 byte, kích thu?c d?u ra c?a SHA-1)
    static unsigned char digest[20];
    // M?ng cho khóa dã d?m và XOR v?i ipad/opad
    unsigned char k_ipad[64], k_opad[64];
    // B? d?m t?m n?u c?n bam khóa
    unsigned char tk[20];

    // Bu?c 1: X? lý khóa
    if (klen > 64) {
        SHA1_CTX ctx;
        SHA1Init(&ctx);
        SHA1Update(&ctx, key, klen);
        SHA1Final(tk, &ctx);
        key = tk;    // S? d?ng khóa dã bam
        klen = 20;   // Ð? dài m?i là 20 byte
    }

    // Ð?m khóa d?n 64 byte và t?o k_ipad, k_opad
    memset(k_ipad, 0, 64);
    memset(k_opad, 0, 64);
    memcpy(k_ipad, key, klen);
    memcpy(k_opad, key, klen);

    // XOR v?i ipad (0x36) và opad (0x5C)
    for (int i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5C;
    }

    // Bu?c 2: Tính bam trong
    SHA1_CTX ctx;
    unsigned char inner_hash[20];
    SHA1Init(&ctx);
    SHA1Update(&ctx, k_ipad, 64);                  // 64 byte t? k_ipad
    SHA1Update(&ctx, (const unsigned char*)&interval, 8); // 8 byte t? interval
    SHA1Final(inner_hash, &ctx);                   // K?t qu?: 20 byte

    // Bu?c 3: Tính bam ngoài
    SHA1Init(&ctx);
    SHA1Update(&ctx, k_opad, 64);                  // 64 byte t? k_opad
    SHA1Update(&ctx, inner_hash, 20);              // 20 byte t? inner_hash
    SHA1Final(digest, &ctx);                       // K?t qu? cu?i cùng vào digest

    // Bu?c 4: Tr? v? con tr? d?n digest
    return digest;
}

static uint32_t dt(uint8_t* digest) {
    // straight from RFC4226 Section 5.4
    uint64_t offset = digest[19] & 0x0F;
    uint32_t bin_code = (digest[offset] & 0x7f) << 24 |
                        (digest[offset+1] & 0xff) << 16 |
                        (digest[offset+2] & 0xff) <<  8 |
                        (digest[offset+3] & 0xff);

    return bin_code;
}

uint32_t hotp(uint8_t* key, size_t klen, uint64_t interval, int digits) {
    // make interval big endian
    uint32_t endianness = 0xdeadbeef; // little trick to coax out memory issues
    if ((*(const uint8_t *)&endianness) == 0xef) {
        interval = ((interval & 0x00000000ffffffff) << 32) | ((interval & 0xffffffff00000000) >> 32);
        interval = ((interval & 0x0000ffff0000ffff) << 16) | ((interval & 0xffff0000ffff0000) >> 16);
        interval = ((interval & 0x00ff00ff00ff00ff) <<  8) | ((interval & 0xff00ff00ff00ff00) >>  8);
    };

    uint8_t* digest = (uint8_t*)hmacsha(key, klen, interval);
    uint32_t dt_bincode = dt(digest);
    uint32_t res = truncateToDigits(dt_bincode, digits);
    return res;
}

double my_floor(double x) {
    if (x >= 0) {
        return (double)((int)x);
    } else {
        if (x == (int)x) { // Ki?m tra xem x có ph?i là s? nguyên không
            return x;
        } else {
            return (double)((int)x - 1);
        }
    }
}


time_t getTime(time_t T0) {
    return my_floor((time(NULL) - T0)/step);
}

uint32_t totp(uint8_t* key, size_t klen, uint64_t time, int digits) {
    uint32_t totp = hotp(key, klen, time, digits);
    return totp;
}