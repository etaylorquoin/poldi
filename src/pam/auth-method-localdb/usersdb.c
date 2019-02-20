/* usersdb.c - PAM authentication via OpenPGP smartcards.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <gcrypt.h>

#include "usersdb.h"
#include "defs-localdb.h"



/* This is the type for callbacks functions, which need to be passed
   to usersdb_process().  The callback function receives one
   (SERIALNO, USERNAME) pair per invocation.  OPAQUE is the opaque
   arugment passed to usersdb_process().  The return code of such a
   callback functions has the following meanings:

   0: Continue processing the users database.
   1: Stop processing.  */
typedef int (*usersdb_cb_t) (const char *serialno, const char *username,
			     void *opaque);

/* This functions processes the users database.  For each read pair of
   a card serial number and a account, the callback function specified
   as (CB, OPAQUE) is called.  Depending on CB's return code,
   processing is continued or aborted.  */
static gpg_error_t
usersdb_process (usersdb_cb_t cb, void *opaque)
{
  const char *delimiters = "\t\n ";
  gpg_error_t err;
  FILE *usersdb;
  char *line;
  char *line_serialno;
  char *line_username;
  size_t line_n;
  ssize_t ret;
  int cb_ret;
  char *save_ptr;		/* For strtok_r. */

  line_serialno = NULL;
  line_username = NULL;
  line = NULL;
  err = 0;

  /* Open users database.  */
  usersdb = fopen (POLDI_USERS_DB_FILE, "r");
  if (! usersdb)
    {
      err = gpg_error_from_syserror ();
      goto out;
    }

  /* Process lines.  */
  while (1)
    {
      /* Get next line.  */
      save_ptr = NULL;
      line = NULL;
      line_n = 0;
      ret = getline (&line, &line_n, usersdb);
      if (ret == -1)
	{
	  if (ferror (usersdb))
	    err = gpg_error_from_syserror ();
	  /* else EOF.  */
	  break;
	}

      {
	/* Cut off comments.  */
	char *comment = strchr (line, '#');
	if (comment)
	  *comment = '\0';
      }

      /* Extract first token: the card serial number. */
      line_serialno = strtok_r (line, delimiters, &save_ptr);
      if (!line_serialno)
	/* Ignore this incomplete entry.  */
	goto skip;

      /* Extract second token: the username. */
      line_username = strtok_r (NULL, delimiters, &save_ptr);
      if (!line_username)
	/* Ignore this incomplete entry.  */
	goto skip;

      /* Looks like a valid entry, pass to callback function.  */
      cb_ret = (*cb) (line_serialno, line_username, opaque);
      if (cb_ret)
	/* Callback functions wants us to stop.  */
	break;

    skip:
      free (line);
    }
  if (err)
    goto out;

  /* Finalize.  */
  (*cb) (NULL, NULL, opaque);

 out:

  if (usersdb)
    fclose (usersdb);
  free (line);			/* Allocated by getline, thus standard
				   free. */
  return err;
}



/*
 * Implementation of "usersdb_check" function.  usersdb_check()
 * figures out wether a given serial number is assocated with a given
 * username or not.
 */

/* Type for opaque callback argument.  */
typedef struct check_cb_s
{
  /* This is the pair we are looking for. */
  const char *serialno;
  const char *username;

  /* If found, this is set to TRUE by the callback function.  */
  int match;
} *check_cb_t;

/* Callback function.  */
static int
usersdb_check_cb (const char *serialno, const char *username, void *opaque)
{
  check_cb_t ctx = opaque;

  if (! (serialno || username))
    /* Finalizing */;
  else
    {
      /* Regular entry.  */

      if ((! strcmp (ctx->serialno, serialno))
	  && (! strcmp (ctx->username, username)))
	{
	  /* The current entry is exactly the one we were looking
	     for.  */
	  ctx->match = 1;
	  return 1;
	}
    }

  return 0;
}

/* This functions figures out wether the provided (SERIALNO, USERNAME)
   pair is contained in the users database.  */
gpg_error_t
usersdb_check (const char *serialno, const char *username)
{
  struct check_cb_s ctx = { serialno, username, 0 };
  gpg_error_t err;

  err = usersdb_process (usersdb_check_cb, &ctx);
  if (! err)
    {
      /* Now we have a result in CTX.  */

      if (! ctx.match)
	err = gpg_error (GPG_ERR_NOT_FOUND); /* FIXME: not the best
						return code...  */
    }

  return err;
}



/*
 *
 */

/* Looking up a username for a given serial number.  */

typedef struct lookup_cb_s
{
  const char *serialno;
  const char *username;
  int matches;
  char *found;
  gpg_error_t err;
} *lookup_cb_t;

static int
usersdb_lookup_cb (const char *serialno, const char *username, void *opaque)
{
  lookup_cb_t ctx = opaque;
  char *str;

  if (! (serialno || username))
    goto out;

  /* FIXME: this function is not nice.  */

  if (ctx->serialno)
    {
      if (! strcmp (ctx->serialno, serialno))
	{
	  if (! ctx->matches)
	    {
	      str = xtrystrdup (username);
	      if (! str)
		{
		  ctx->err = gpg_error_from_syserror ();
		  return 1;
		}

	      ctx->found = str;
	      ctx->matches++;
	    }
	  else
	    {
	      if (strcmp (ctx->found, username))
		ctx->matches++;
	    }
	}
    }
  else
    {
      if (! strcmp (ctx->username, username))
	{
	  if (! ctx->matches)
	    {
	      str = xtrystrdup (serialno);
	      if (! str)
		{
		  ctx->err = gpg_error_from_syserror ();
		  return 1;
		}

	      ctx->found = str;
	      ctx->matches++;
	    }
	  else
	    {
	      if (strcmp (ctx->found, serialno))
		ctx->matches++;
	    }
	}
    }

 out:

  return 0;
}

/* This function tries to lookup a username by it's serial number;
   this is only possible in case the specified serial number SERIALNO
   is associated with exactly one username.  The username will be
   stored in newly allocated memory in *USERNAME.  Returns proper
   error code.  */
gpg_error_t
usersdb_lookup_by_serialno (const char *serialno, char **username)
{
  struct lookup_cb_s ctx = { serialno, NULL, 0, NULL, 0 };
  gpg_error_t err;

  assert (serialno);
  assert (username);

  err = usersdb_process (usersdb_lookup_cb, &ctx);
  if (err)
    goto out;

  /* Now we have a result in CTX.  */

  if (ctx.err)
    {
      err = ctx.err;
      goto out;
    }

  if (! ctx.matches)
    {
      err = gpg_error (GPG_ERR_NOT_FOUND);
      goto out;
    }
  else if (ctx.matches > 1)
    {
      err = gpg_error (GPG_ERR_AMBIGUOUS_NAME);
      goto out;
    }
  else if (ctx.matches == 1)
    {
      *username = ctx.found;
      ctx.found = NULL;
    }

 out:

  xfree (ctx.found);

  return err;
}

/* This function tries to lookup a serial number by it's username;
   this is only possible in case the specified username USERNAME is
   associated with exactly one serial number.  The serial number will
   be stored in newly allocated memory in *SERIALNO.  Returns proper
   error code.  */
gpg_error_t
usersdb_lookup_by_username (const char *username, char **serialno)
{
  struct lookup_cb_s ctx = { NULL, username, 0, NULL, 0 };
  gpg_error_t err;

  assert (username);
  assert (serialno);

  err = usersdb_process (usersdb_lookup_cb, &ctx);
  if (err)
    goto out;

  /* Now we have a result in CTX.  */

  if (ctx.err)
    {
      err = ctx.err;
      goto out;
    }

  if (! ctx.matches)
    {
      err = gpg_error (GPG_ERR_NOT_FOUND);
      goto out;
    }
  else if (ctx.matches > 1)
    {
      err = gpg_error (GPG_ERR_AMBIGUOUS_NAME);
      goto out;
    }
  else if (ctx.matches == 1)
    {
      *serialno = ctx.found;
      ctx.found = NULL;
    }

 out:

  xfree (ctx.found);

  return err;
}

/* END */
