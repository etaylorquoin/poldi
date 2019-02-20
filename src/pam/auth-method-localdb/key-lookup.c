/* key-lookup.c - Lookup keys for localdb authentication
   Copyright (C) 2004, 2005, 2007, 2008 g10 Code GmbH

   This file is part of Poldi.

   Poldi is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   Poldi is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <poldi.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <gpg-error.h>
#include <gcrypt.h>

#include "util/support.h"
#include "util/filenames.h"
#include "key-lookup.h"
#include "defs-localdb.h"


/* This functions construct a new C-string containing the absolute
   path for the file, which is to expected to contain the public key
   for the card identified by SERIALNO.  Returns proper error
   code.  */
static gpg_error_t
key_filename_construct (char **filename, const char *serialno)
{
  return make_filename (filename, POLDI_KEY_DIRECTORY, serialno, NULL);
}

/* Lookup the key belonging to the card specified by SERIALNO.
   Returns a proper error code.  */
gpg_error_t
key_lookup_by_serialno (poldi_ctx_t ctx, const char *serialno, gcry_sexp_t *key)
{
  gcry_sexp_t key_sexp;
  char *key_string;
  char *key_path;
  gpg_error_t err;

  key_path = NULL;
  key_string = NULL;

  err = key_filename_construct (&key_path, serialno);
  if (err)
    {
      log_msg_error (ctx->loghandle,
		     "failed to construct key file path "
		     "for serial number `%s': %s\n",
		     serialno, gpg_strerror (err));
      goto out;
    }

  err = file_to_string (key_path, &key_string);
  if ((! err) && (! key_string))
    err = gpg_error (GPG_ERR_NO_PUBKEY);
  if (err)
    {
      log_msg_error (ctx->loghandle,
		     "failed to retrieve key from key file `%s': %s\n",
		     key_path, gpg_strerror (err));
      goto out;
    }

  err = string_to_sexp (&key_sexp, key_string);
  if (err)
    {
      log_msg_error (ctx->loghandle,
		     "failed to convert key "
		     "from `%s' into S-Expression: %s\n",
		     key_path, gpg_strerror (err));
      goto out;
    }

  *key = key_sexp;

 out:

  xfree (key_path);
  xfree (key_string);

  return err;
}

/* Lookup key type ex, rsa, ecc, returns 0 on error and 1 otherwise
   Currenly Only Supporting rsa and ed25519*/
int get_key_type(poldi_ctx_t ctx, key_types *key_type, gcry_sexp_t key) {
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

  //unsupported crypto
  log_msg_error (ctx->loghandle,
     "Unsupported Key Type "
     "`%s'\n", token);

  return 0;
}//key_type


