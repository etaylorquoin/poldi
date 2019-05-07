#!/bin/sh 

session_bin='session-test'
preloader_cmd='LD_PRELOAD=libpam_wrapper.so PAM_WRAPPER=1 PAM_WRAPPER_USE_SYSLOG=0 PAM_WRAPPER_DEBUGLEVEL=2 PAM_WRAPPER_SERVICE_DIR=./services ./session-test'

if  [ -f "${session_bin}" ]
then
   printf '\nStarting libpam with preloader . . .\n\n'
   eval "${preloader_cmd} $1"
else
   printf 'Error Please compile session-test\n'
fi

