/* support.c - PAM authentication via OpenPGP smartcards.
   Copyright (C) 2004, 2005, 2007, 2008 g10 Code GmbH
 
   This file is part of Poldi.
  
   Poldi is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   Poldi is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "util-local.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <pwd.h>
#include <dirent.h>
#include <limits.h>

#include <gcrypt.h>

#include "support.h"
#include "defs.h"

#define CHALLENGE_MD_ALGORITHM GCRY_MD_SHA1



/* Note: it's expected that xtrymalloc, xtrystrdup and xfree are
   defined in util-local.h. */

/* This function generates a challenge; the challenge will be stored
   in newly allocated memory, which is to be stored in *CHALLENGE;
   it's length in bytes is to be stored in *CHALLENGE_N.  Returns
   proper error code.  */
gpg_error_t
challenge_generate (unsigned char **challenge, size_t *challenge_n)
{
  gpg_error_t err = GPG_ERR_NO_ERROR;
  unsigned char *challenge_new = NULL;
  size_t challenge_new_n = gcry_md_get_algo_dlen (CHALLENGE_MD_ALGORITHM);

  challenge_new = xtrymalloc (challenge_new_n);
  if (! challenge_new)
    err = gpg_err_code_from_errno (errno);
  else
    {
      gcry_create_nonce (challenge_new, challenge_new_n);
      *challenge = challenge_new;
      *challenge_n = challenge_new_n;
    }

  return err;
}

/* Releases the challenge contained in CHALLENGE generated by
   challenge_generate().  */
void
challenge_release (unsigned char *challenge)
{
  if (challenge)
    xfree (challenge);
}

static gpg_error_t
challenge_verify_sexp (gcry_sexp_t sexp_key,
		       unsigned char *challenge, size_t challenge_n,
		       unsigned char *response, size_t response_n)
{
  gpg_error_t err = GPG_ERR_NO_ERROR;
  gcry_sexp_t sexp_signature = NULL;
  gcry_sexp_t sexp_data = NULL;
  gcry_mpi_t mpi_signature = NULL;

  /* Convert buffers into MPIs.  */
  if (! err)
    {
      if (gcry_mpi_scan (&mpi_signature, GCRYMPI_FMT_USG, response, response_n,
			 NULL))
	err = gpg_error (GPG_ERR_BAD_MPI);
    }

  /* Create according S-Expressions.  */
  if (! err)
    err = gcry_sexp_build (&sexp_data, NULL,
			   "(data (flags pkcs1) (hash sha1 %b))",
			   challenge_n, challenge);
  if (! err)
    err = gcry_sexp_build (&sexp_signature, NULL, "(sig-val (rsa (s %m)))",
			   mpi_signature);

  /* Verify.  */
  if (! err)
    err = gcry_pk_verify (sexp_signature, sexp_data, sexp_key);

  if (sexp_data)
    gcry_sexp_release (sexp_data);
  if (sexp_signature)
    gcry_sexp_release (sexp_signature);
  if (mpi_signature)
    gcry_mpi_release (mpi_signature);

  return err;
}

/* This functions verifies that the signature contained in RESPONSE of
   size RESPONSE_N (in bytes) is indeed the result of signing the
   challenge given in CHALLENGE of size CHALLENGE_N (in bytes) with
   the secret key belonging to the public key given as PUBLIC_KEY.
   Returns proper error code.  */
gpg_error_t
challenge_verify (gcry_sexp_t public_key,
		  unsigned char *challenge, size_t challenge_n,
		  unsigned char *response, size_t response_n)
{
  gpg_error_t err;

  err = challenge_verify_sexp (public_key,
			       challenge, challenge_n, response, response_n);

  return err;
}



/*
 * S-Expression conversion.
 */

/* This function converts the given S-Expression SEXP into it's
   `ADVANCED' string representation, using newly-allocated memory,
   storing the resulting NUL-terminated string in *SEXP_STRING.
   Returns a proper error code.  */
gpg_error_t
sexp_to_string (gcry_sexp_t sexp, char **sexp_string)
{
  const int fmt = GCRYSEXP_FMT_ADVANCED;
  gpg_error_t err;
  size_t buffer_n;
  char *buffer;

  assert (sexp);

  buffer = NULL;

  /* Figure out amount of memory required for
     string-representation.  */
  buffer_n = gcry_sexp_sprint (sexp, fmt, NULL, 0);
  if (! buffer_n)
    {
      err = gpg_error (GPG_ERR_INV_SEXP); /* ? */
      goto out;
    }

  /* Allocate memory.  */
  buffer = xtrymalloc (buffer_n);
  if (!buffer)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  /* And write string-representation into buffer.  */
  buffer_n = gcry_sexp_sprint (sexp, fmt, buffer, buffer_n);
  if (!buffer_n)
    {
      err = gpg_error (GPG_ERR_INV_SEXP); /* ? */
      goto out;
    }

  *sexp_string = buffer;
  err = 0;
  
 out:

  if (err)
    xfree (buffer);

  return err;
}

/* This functions converts the given string-representation of an
   S-Expression into a new S-Expression object, which is to be stored
   in *SEXP.  Returns proper error code.  */
gpg_error_t
string_to_sexp (gcry_sexp_t *sexp, char *string)
{
  gpg_error_t err;

  err = gcry_sexp_sscan (sexp, NULL, string, strlen (string));

  return err;
}

/* This function retrieves the content from the file specified by
   FILENAMED and writes it into a newly allocated chunk of memory,
   which is then stored in *DATA and *DATALEN.  This functions adds a
   NUL termination to the actual file data but does not include that
   additional NUL character in DATALEN!  Returns proper error
   code.  */
static gpg_error_t
file_to_string_internal (const char *filename, void **data, size_t *datalen)
{
  struct stat statbuf;
  unsigned char *data_new;
  gpg_error_t err;
  FILE *fp;
  int ret;

  data_new = NULL;
  fp = NULL;

  /* Retrieve file size.  */
  ret = stat (filename, &statbuf);
  if (ret)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  if (statbuf.st_size)
    {
      fp = fopen (filename, "r");
      if (! fp)
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}
      data_new = xtrymalloc (statbuf.st_size + 1);
      if (!data_new)
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}
      ret = fread (data_new, statbuf.st_size, 1, fp);
      if (ret != 1)
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}
      data_new[statbuf.st_size] = 0;
    }

  err = 0;
  *data = data_new;
  if (datalen)
    *datalen = statbuf.st_size;

 out:

  if (fp)
    fclose (fp);

  if (err)
    xfree (data_new);

  return err;
}


/* This function retrieves the content from the file specified by
   FILENAMED and writes it into a newly allocated chunk of memory,
   which is then stored in *DATA and *DATALEN.  Returns proper error
   code.  */
gpg_error_t
file_to_binstring (const char *filename, void **data, size_t *datalen)
{
  return file_to_string_internal (filename, data, datalen);
}

/* This function retrieves the content from the file specified by
   FILENAMED and writes it into a newly allocated C-string, which is
   then stored in *STRING.  Returns proper error code.  */
gpg_error_t
file_to_string (const char *filename, char **string)
{
  gpg_error_t err;
  void *data;

  err = file_to_string_internal (filename, &data, NULL);
  if (err)
    goto out;

  *string = data;

 out:

  return err;
}



gpg_error_t
char_vector_dup (int len, const char **a, char ***b)
{
  char **c;
  gpg_error_t err;
  int i;

  c = NULL;
  err = 0;

  c = xtrymalloc (sizeof (*c) * (len + 1));
  if (!c)
    {
      err = gpg_error_from_errno (errno);
      goto out;
    }

  for (i = 0; i < len + 1; i++)
    c[i] = NULL;

  for (i = 0; i < len; i++)
    {
      c[i] = xtrystrdup (a[i]);
      if (!c[i])
	{
	  err = gpg_error_from_errno (errno);
	  goto out;
	}
    }
  c[i] = NULL;

 out:

  if (err)
    {
      if (c)
	{
	  for (i = 0; c[i]; i++)
	    xfree (c[i]);
	  xfree (c);
	}
      *b = NULL;
    }
  else
    *b = c;

  return err;
}

void
char_vector_free (char **a)
{
  int i;

  if (a)
    {
      for (i = 0; a[i]; i++)
	xfree (a[i]);
      xfree (a);
    }
}

int
my_strlen (const char *s)
{
  int ret = 0;
  
  while (*s)
    {
      if (ret == INT_MAX)
	{
	  ret = -1;
	  break;
	}
      ret++;
      s++;
    }

  return ret;
}


/* Copied from gnome-keyring */
void wipestr(char *data) {
    volatile char *vp;
    size_t len;
    if (!data) {
        return;
    }
    /* Defeats some optimizations */
    len = strlen(data);
    memset(data, 0xAA, len);
    memset(data, 0xBB, len);
    /* Defeats others */
    vp = (volatile char*) data;
    while (*vp) {
        *(vp++) = 0xAA;
    }
    gcry_free((void *) data);
    
}

/* END */
