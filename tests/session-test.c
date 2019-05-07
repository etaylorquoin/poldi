#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <gpg-error.h>
#include <libpamtest.h>

#include <simpleparse.h>
#include <simplelog.h>
#include "scd/scd.h"

#define MAX_LENGTH      512
#define str(x)          # x
#define xstr(x)         str(x)

#define PROGRAM_NAME    "session-test"
#define PROGRAM_VERSION "0.1"

//void test_scd_connect(const char *username, log_handle_t loghandle, pam_handle_t *pam_handle);
static void print_help(void);
static void print_version (void);

int main (int argc, const char *argv[])
{
	gpg_error_t err = 0;
	log_handle_t loghandle = NULL;
	simpleparse_handle_t handle = NULL;
	pam_handle_t *pam_handle = NULL;
	const char *username = NULL;

	int rt_val;

	assert (argc > 0);

	err = log_create (&loghandle);
	assert (!err);

	err = log_set_backend_stream (loghandle, stderr);
	assert (!err);

	if (argc != 2)
	{
		print_help();
		return 1;
	}
	else
	{

	}

	while(1)
	{
		static struct option long_options[] =
		{
		  { "version", no_argument, 0, 'v' },
		  { "help", no_argument, 0, 'h' },
		  { "user", required_argument, 0, 'u' },
		  { 0, 0, 0, 0 }
		};

		int index = 0;

		rt_val = getopt_long (argc, argv, "vh:",
			       long_options, &index);

		//end of options
		if (rt_val == -1)
		{
			 break;
		}

		switch (rt_val)
		{
		case 'u':
			username = strdup (optarg);
			if (!username)
			{
			  fprintf (stderr, "failed to duplicate username: %s", strerror (errno));
			  exit (1);
			}
			break;

		case 'h':
			print_help();
			exit(0);
			break;

		case 'v':
			print_version();
			exit(0);
			break;

		case '?':
			break;//error

		default:
			abort();
		}//switch



	}//while

	if (argc - optind != 1)
	{
	  print_help ();
	  exit (1);
	}

	username = argv[optind];

	printf("Testing for user %s \n", username);
	printf("Testing poldi-session \n");

	//get test pin from user
	char buff[MAX_LENGTH];
	printf("Enter pin: ");
	scanf("%"xstr(MAX_LENGTH)"s", &buff);
	printf("Buff: %s\n" , &buff);

	enum pamtest_err perr;
	const char *new_authtoks[] = {
	        "123456",              /* login pin */
	        NULL,
	};
	struct pamtest_conv_data conv_data = {
	     .in_echo_off = new_authtoks,
	};
	struct pam_testcase tests[] = {
	    /* pam function to execute and expected return code */
	    pam_test(PAMTEST_AUTHENTICATE, PAM_SUCCESS),
		pam_test(PAMTEST_OPEN_SESSION, PAM_SUCCESS),
	};

	perr = run_pamtest("matrix.in",             /* PAM service */
	                   "etaylor",                /* user logging in */
	                    &conv_data, tests);  /* conversation data and array of tests */

	printf("err = %d\n", perr);

	switch (perr)
	{
	   case PAMTEST_ERR_OK:
	      printf("Pass\n");
	      break;

	   case PAMTEST_ERR_START:
	     printf("Error Starting pam\n");
	      break;

	   case PAMTEST_ERR_CASE:
	      printf("Error couldn't run test case\n");
	      break;

	   case PAMTEST_ERR_OP:
	      printf("Error couldn't run test case\n");
	      break;

	   case PAMTEST_ERR_END:
	      printf("Error pam end faild\n");
	      break;

	   case PAMTEST_ERR_KEEPHANDLE:
	      printf("Error internal\n");
	      break;

	   case PAMTEST_ERR_INTERNAL:
	      printf("Error internal\n");
	      break;

	   default:
	     printf("Error unknown\n");

	   };



	//test_scd_connect(username, loghandle, NULL);

	return 0;

}//main


static void print_help (void)
{
  printf ("\
Usage: %s [options] <PAM user name>\n\
Test PAM session.\n\
\n\
Options:\n\
 -h, --help      print help information\n\
 -v, --version   print version information\n\
\n", PROGRAM_NAME);
}


static void print_version (void)
{
  printf (PROGRAM_NAME " " PROGRAM_VERSION "\n");
}

//void test_scd_connect(const char *username, log_handle_t loghandle, pam_handle_t *pam_handle)
//{
//	gpg_error_t err = 0;
//
//	err = log_set_backend_stream (loghandle, stderr);
//	assert (!err);
//
//	scd_context_t scd_ctx;
//	int use_agent = 2;
//
//	struct passwd pwd, *result;
//	char *buf = NULL;
//	size_t bufsize;
//
//	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
//	if (bufsize == -1)
//	{
//		bufsize = 16384;
//	}
//
//	//allocate and get users passwd strcut
//	buf = (char*) malloc(bufsize);
//	ret = getpwnam_r(pam_username, &pwd, buf, bufsize, &result);
//}

