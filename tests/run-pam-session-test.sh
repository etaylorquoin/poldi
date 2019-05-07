#!/bin/sh 

session_bin='session-test'
preloader_cmd='LD_PRELOAD=libpam_wrapper.so PAM_WRAPPER=1 PAM_WRAPPER_USE_SYSLOG=0 PAM_WRAPPER_DEBUGLEVEL=3 PAM_WRAPPER_SERVICE_DIR=./services ./session-test'

if  [ -f "${session_bin}" ]
then
   eval "${preloader_cmd}"
else
   printf 'Error Please compile session-test\n'
fi

