#ifndef _BIGNUM_UTILS_H_
#define _BIGNUM_UTILS_H_

/** Helper macro to create a big number from string */
#define INIT_CHECK(n, s) (n) = create_BN_state_from_string((s)); \
    if((n) == NULL)                                                 \
    {                                                            \
        printf("Could not init big num\n");                     \
        return 0;                                                \
    }

/** Get maximum of two numbers */
#define MAX(a,b) (((a)>(b))?(a):(b))

/** 
 * Create a big number
 * 
 * @param len Length of provided data
 * @param pData Provided data
 * @return NULL on failure, big number state otherwise
 */
IppsBigNumState* create_BN_state(int len, const Ipp32u* pData) {
  int size;

  ippsBigNumGetSize(len, &size);
  IppsBigNumState* pBN = (IppsBigNumState*) malloc(size);

  if(!pBN) {
    return NULL;
  }

  ippsBigNumInit(len, pBN);

  if(pData != NULL) {
    ippsSet_BN(IppsBigNumPOS, len, pData, pBN);
  }

  return pBN;
}

/**
 * Create a big number from a number encoded as hexadecimal string (with leading 0x).
 * 
 * @param s Hexadecimal string representation of a big number
 * @return NULL on failure, big number state otherwise
 */
IppsBigNumState* create_BN_state_from_string(const char* s) {
  const char HexDigitList[] = "0123456789ABCDEF";
  if(!('0' == s[0]) && (('x' == s[1]) || ('X' == s[1]))) {
    return NULL;
  }
  s += 2;

  const size_t len_in_nibbles = strlen(s);
  // Must be dword aligned, add leading zeroes if not
  if((len_in_nibbles % 8) != 0) {
    return NULL;
  }

  const size_t len_in_dword = (int)(len_in_nibbles + 7) / 8;

  Ipp32u* num = calloc(len_in_dword, sizeof(Ipp32u));
  if(!num) {
    return NULL;
  }

  int nibble_in_dword = 7;
  Ipp32u current_dword = 0;
  int dword_idx = len_in_dword - 1;

  for(unsigned int i = 0; i < len_in_nibbles; i++) {
    // Convert digit to hex
    const char tmp[2] = {s[i], 0};
    Ipp32u digit = (Ipp32u)strcspn(HexDigitList, tmp);

    current_dword |= digit << 4*nibble_in_dword;

    if(nibble_in_dword == 0) {
      nibble_in_dword = 7;
      num[dword_idx] = current_dword;
      current_dword = 0;
      dword_idx--;
    } else {
      nibble_in_dword--;
    }
  }

  IppsBigNumState* result = create_BN_state(len_in_dword, num);

  free(num);
  return result;
}
 
#endif
