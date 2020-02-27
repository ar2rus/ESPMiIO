#ifndef crypto_utils_H
#define crypto_utils_H

#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_block.h>

#include <Arduino.h>

void br_md5(char* buffer, size_t size, char* digest){
	br_md5_context _ctx;
	br_md5_init(&_ctx);
	br_md5_update(&_ctx, buffer, size);
	br_md5_out(&_ctx, digest);
}

void br_aes_ct_cbcenc(char* key, size_t key_len, char* iv, char* data, size_t data_len){
  br_aes_ct_cbcenc_keys _ctx;
  br_aes_ct_cbcenc_init(&_ctx, key, key_len);
  br_aes_ct_cbcenc_run(&_ctx, iv, data, data_len);
}

void br_aes_ct_cbcdec(char* key, size_t key_len, char* iv, char* data, size_t data_len){
  br_aes_ct_cbcdec_keys _ctx;
  br_aes_ct_cbcdec_init(&_ctx, key, key_len);
  br_aes_ct_cbcdec_run(&_ctx, iv, data, data_len);
}

#endif
