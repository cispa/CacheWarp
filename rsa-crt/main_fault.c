#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "ippcp.h"
#include "utils.h"

/** Length of RSA key */
#define RSA_LEN 256

/** Parameters for RSA signature */
static IppsBigNumState *P = 0, *Q = 0, *dP = 0, *dQ = 0, *invQ = 0, *D = 0, *N = 0, *E = 0;

/** Private key */
static IppsRSAPrivateKeyState* pPrv = 0;
/** Public key */
static IppsRSAPublicKeyState* pPub = 0;
/** Temporary buffer */
static Ipp8u* scratchBuffer = 0;

/**
 * Load the private key from the given file.
 * The key file is a plaintext file with the hexadecimal string representation of all parameters in the form
 * P; Q; dP; dQ; invQ; N; D; E
 * 
 * @param fname File name of private key
 * @return 0 on success, 1 otherwise
 */
int load_keyfile(const char* fname) {
  FILE* fd = fopen(fname, "r");
  if(!fd) {
      return 1;
  }
  fseek(fd, 0, SEEK_END);
  size_t filelen = ftell(fd);
  rewind(fd);

  char* file_content = calloc(filelen + 1, 1);
  size_t bytes_read = fread(file_content, 1, filelen, fd);
  if (bytes_read != filelen) {
    return 1;
  }

  char delim[] = ";";
  // keyfile format: P; Q; dP; dQ; invQ; N; D; E

  char* tokenptr = strtok(file_content, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(P, tokenptr);

  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(Q, tokenptr);

  // dP = d mod (p-1)
  tokenptr = strtok(NULL, delim);
  if (!tokenptr) return 1;
  INIT_CHECK(dP, tokenptr);

  // dQ = d mod (q-1)
  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(dQ, tokenptr);

  // invQ = modinv(q) mod p
  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(invQ, tokenptr);

  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(N, tokenptr);

  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(D, tokenptr);

  tokenptr = strtok(NULL, delim);
  if(!tokenptr) return 1;
  INIT_CHECK(E, tokenptr);

  return 0;
}


/**
 * Load the private key and generates the internal representation for the public and private key.
 * 
 * @return 0 on success, 1 otherwise
 */
int load_key() {
  int ret = load_keyfile("privatekey.csv");
  if(ret) {
    fprintf(stderr, "[-] Error loading keyfile\n");
    return 1;
  }

  int keyCtxSize;

  // (bit) size of key components
  int bitsN = RSA_LEN * 8;
  int bitsE = 8;
  int bitsP = RSA_LEN * 4;
  int bitsQ = RSA_LEN * 4;

  // define and setup public key
  ippsRSA_GetSizePublicKey(bitsN, bitsE, &keyCtxSize);
  pPub = (IppsRSAPublicKeyState*)calloc(keyCtxSize, sizeof(Ipp8u));

  if(!pPub) {
    return 1;
  }

  ippsRSA_InitPublicKey(bitsN, bitsE, pPub, keyCtxSize);
  ippsRSA_SetPublicKey(N, E, pPub);

  // define and setup (type2) private key
  ippsRSA_GetSizePrivateKeyType2(bitsP, bitsQ, &keyCtxSize);
  pPrv = (IppsRSAPrivateKeyState*)calloc(keyCtxSize, sizeof(Ipp8u));

  if(!pPrv) {
    return 1;
  }

  ippsRSA_InitPrivateKeyType2(bitsP, bitsQ, pPrv, keyCtxSize);
  ippsRSA_SetPrivateKeyType2(P, Q, dP, dQ, invQ, pPrv);

  int buffSizePublic;
  ippsRSA_GetBufferSizePublicKey(&buffSizePublic, pPub);
  int buffSizePrivate;
  ippsRSA_GetBufferSizePrivateKey(&buffSizePrivate, pPrv);
  int buffSize = MAX(buffSizePublic, buffSizePrivate);
  scratchBuffer = calloc(buffSize, sizeof(Ipp8u));

  if(!scratchBuffer) {
    return 1;
  }

  return 0;
}


/**
 * Free all allocated buffers.
 */
void cleanup() {
  free(scratchBuffer);
  free(pPub);
  free(pPrv);
  free(P);
  free(Q);
  free(dP);
  free(dQ);
  free(invQ);
  free(N);
  free(D);
  free(E);
}


/**
 * Sign a message with the loaded key.
 * 
 * @param message The message to sign
 * @param result The signature
 * @return 0 on success
 */
int sign(IppsBigNumState* message, uint8_t result[RSA_LEN]) {
  IppsBigNumSGN sgn;
  IppsBigNumState* sig = create_BN_state(RSA_LEN / 4, 0);

  ippsRSA_Decrypt(message, sig, pPrv, scratchBuffer);

  int result_len = RSA_LEN / 4;
  ippsGet_BN(&sgn, &result_len, (Ipp32u*) result, sig);

  while (result[0] == 134 || result[1] == 3) {
    ippsRSA_Decrypt(message, sig, pPrv, scratchBuffer);
    ippsGet_BN(&sgn, &result_len, (Ipp32u*) result, sig);
  }

  free(sig);
  return 0;
}


/**
 * Save the computed signature to a file.
 * 
 * @param fname File name where the signature is stored
 * @param signature The RSA signature
 * @return 0 on success
 */
int save_signature(const char* fname, uint8_t* signature) {
  FILE* fd = fopen(fname, "w");
  printf("[+] Signature: ");
  for(size_t i = 0; i < RSA_LEN; i++) {
    printf("%.2x ", signature[i]);
    fprintf(fd, "%.2x ", signature[i]);
  }
  printf("\n");
  fclose(fd);
  return 0;
}


/**
 * Tool to sign a message using the RSA key stored in privatekey.csv.
 * The signature is stored in the given file.
 */
int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s <hex message> <signature-file>\n", argv[0]);
    exit(0);
  }
  char* signature_fname = argv[2];
  
  //
  // Initialize
  //
  int ret = load_key();
  if (ret) {
    printf("[-] Error during initialization of key\n");
    exit(0);
  }
  
  //
  // Initialize message
  //
  IppsBigNumState* msg = 0;
  INIT_CHECK(msg, argv[1]);

  //
  // Sign message
  //
  uint8_t buffer[RSA_LEN] = { 0 };
  sign(msg, buffer);

  //
  // Save signature
  //
  save_signature(signature_fname, buffer);

  cleanup();
  free(msg);
  printf("[+] Finished.\n");
  return 0;
}