#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>
#include <stddef.h>

#define AES_BLOCKLEN 16 // Block length in bytes - AES is 128b block only
#define AES_KEYLEN 32   // Key length in bytes

void AES256_CTR(const uint8_t* key, const uint8_t* iv, uint8_t* data, size_t length);

#endif //_AES_H_
