#include "key-types.h"

/* Lookup key type ex, rsa, ecc, returns 0 on error and 1 otherwise
   Currenly Only Supporting rsa and ed25519*/
int get_key_type(key_types *key_type, gcry_sexp_t key) {
  int err = 0;
  size_t strCnt = 0;
  size_t maxBuffSize = 4098;
  char strbuff[maxBuffSize];
  char *token;
  char rsa[] = "rsa";
  char ecc[] = "ecc";

  if(key == NULL) {
    return 0;
  }//if

  strCnt = gcry_sexp_sprint(key, GCRYSEXP_FMT_DEFAULT, strbuff, maxBuffSize);
  if(strCnt == 0) {
    return 0;
  }//if


  //parse key type from S Expression
  token = strtok(strbuff, "\n");
  token = strtok(NULL, ":");
  token = strtok(NULL, "\n");

  //if rsa key
  err = strcmp(token, rsa);
  if(err == 0)
  {
    *key_type = kType_rsa;
    return 1;
  }//if

  //if ecc key
  err = strcmp(token, ecc);
  if(err == 0)
  {
    *key_type = kType_ecc_Ed25519;
    return 1;
  }//if


  return 0;
}//key_type

/* Returns the String Value of the key type
   Returns Empty String on error*/
const char* key_type_to_str(key_types key_type)
{
  switch (key_type)
  {
    case kType_rsa: return "rsa";
    case kType_ecc_Ed25519: return "Ed25519";
  }

  return "";
}
