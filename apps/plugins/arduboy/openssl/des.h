#ifndef ARDUBOY_STUB_OPENSSL_DES_H
#define ARDUBOY_STUB_OPENSSL_DES_H

typedef unsigned char DES_cblock[8];
typedef const unsigned char const_DES_cblock[8];
typedef struct DES_ks {
    unsigned char unused;
} DES_key_schedule;

#define DES_ENCRYPT 1
#define DES_DECRYPT 0

static inline void DES_set_key_unchecked(const_DES_cblock *key,
                                         DES_key_schedule *schedule)
{
    (void)key;
    (void)schedule;
}

static inline void DES_ecb_encrypt(const_DES_cblock *input,
                                   DES_cblock *output,
                                   DES_key_schedule *schedule,
                                   int enc)
{
    (void)schedule;
    (void)enc;
    (*output)[0] = (*input)[0];
    (*output)[1] = (*input)[1];
    (*output)[2] = (*input)[2];
    (*output)[3] = (*input)[3];
    (*output)[4] = (*input)[4];
    (*output)[5] = (*input)[5];
    (*output)[6] = (*input)[6];
    (*output)[7] = (*input)[7];
}

#endif
