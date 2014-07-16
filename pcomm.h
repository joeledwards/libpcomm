/*
 *  PComm
 *  Copyright (C) 2010, 2012, 2014
 *
 *  Contributor(s):
 *      Joel Edwards <joel.edwards@gmail.com>
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms is hereby granted without a
 * fee.  THIS SOFTWARE IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * THE AUTHOR SHALL NOT BE HELD LIABLE FOR ANY DAMAGES RESULTING FROM
 * THE USE OF THIS SOFTWARE, EITHER DIRECTLY OR INDIRECTLY, INCLUDING,
 * BUT NOT LIMITED TO, LOSS OF DATA OR DATA BEING RENDERED INACCURATE.
 *
 */
#ifndef __PCOMM_H__
#define __PCOMM_H__

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/resource.h>

#include "simclist.h"


/* * * * * * * * * * * * * * * *
 * Constants and Enumerations  *
 * * * * * * * * * * * * * * * */

#define PCOMM_PAGE_SIZE 4096

#define PCOMM_STDIN  0
#define PCOMM_STDOUT 1
#define PCOMM_STDERR 2

/* Result Codes */
enum PCOMM_RESULT {
    /* 0 */
    PCOMM_SUCCESS = 0,
    PCOMM_INIT_FAILED,
    PCOMM_DESTROY_FAILED,
    PCOMM_FD_OPEN_FAILED,
    PCOMM_FD_READ_FAILED,
    /* 5 */
    PCOMM_FD_WRITE_FAILED,
    PCOMM_FD_CLOSE_FAILED,
    PCOMM_FD_NOT_FOUND,
    PCOMM_FD_NEGATIVE,
    PCOMM_FD_EOF_REACHED,
    /* 10 */
    PCOMM_INDEX_NOT_FOUND,
    PCOMM_LIST_REMOVE_FAILED,
    PCOMM_LIST_BAD_SIZE,
    PCOMM_NO_DATA_FROM_READ,
    PCOMM_NO_DATA_FOR_WRITE,
    /* 15 */
    PCOMM_OUT_OF_MEMORY,
    PCOMM_TERMINATED_BY_ERROR,
    PCOMM_UNINITIALIZED_CONTEXT,
    PCOMM_NULL_CONTEXT,
    PCOMM_NULL_CALLBACK,
    /* 20 */
    PCOMM_NULL_LIST,
    PCOMM_NULL_FD,
    PCOMM_NULL_BUFFER,
    PCOMM_DUPLICATE_FD,
    PCOMM_INVALID_STREAM_TYPE,
    /* 25 */
    PCOMM_EXITING
};
typedef enum PCOMM_RESULT pcomm_result_t;

/* Default stream identifiers */
enum PCOMM_STREAM {
    PCOMM_STREAM_WRITE,
    PCOMM_STREAM_READ,
    PCOMM_STREAM_ERROR
};
typedef enum PCOMM_STREAM pcomm_stream_t;


/* * * * * * * * * * * * * * * * *
 * Callback Function Prototypes  *
 * * * * * * * * * * * * * * * * */

struct PCOMM_CONTEXT;
typedef struct PCOMM_CONTEXT pcomm_context_t;

struct PCOMM_FD;
typedef struct PCOMM_FD pcomm_fd_t;

/* pcomm_callback_ready is called when a descriptor has detected an I/O
 * event, such as a close, or I/O can now be sent/received
 */
typedef void (* pcomm_callback_ready)(pcomm_context_t *context, int fd);

/* pcomm_callback_io is called after handling I/O events on a descriptor */
typedef void (* pcomm_callback_io)(pcomm_context_t *context, int fd, uint8_t *data, size_t length);

/* pcomm_callback_routine is called for maintenance events
 * - select operations times out
 * - before select is called
 * - after select succeeds (not when it times out)
 */
typedef void (* pcomm_callback_routine)(pcomm_context_t *context);


/* * * * * * * * * * * * * *
 * Context Data Stuctures  *
 * * * * * * * * * * * * * */

/* The pcomm context object used for managing all file descriptors and program
 * state information.
 */
struct PCOMM_CONTEXT {
    list_t read_fds;
    list_t write_fds;
    list_t error_fds;

    int initialized;
    size_t page_size;
    void* external_context;

    pcomm_callback_routine prepare_callback;
    pcomm_callback_routine select_callback;
    pcomm_callback_routine timeout_callback;
    struct timeval timeout;

    int debug;
    int exit_now;
    int exit_request;
}; // pcomm_context_t

/* The file descriptor context object, used for tracking each individual
 * file descriptor.
 */
struct PCOMM_FD {
    int file_descriptor;
    pcomm_callback_ready ready_callback;
    pcomm_callback_io io_callback;
    pcomm_callback_ready close_callback;

    void* external_context;

    uint8_t *buffer;
    size_t length;
    size_t used;
    size_t offset;
    int last_read_empty;
    int check_only;
}; // pcomm_fd_t


/* * * * * * * * * * * * * * *
 * API Function Declarations *
 * * * * * * * * * * * * * * */

/* context creation and destruction */
pcomm_result_t pcomm_init( pcomm_context_t *context );
pcomm_result_t pcomm_destroy( pcomm_context_t *context );

/* main loop control */
pcomm_result_t pcomm_main( pcomm_context_t *context );
pcomm_result_t pcomm_stop( pcomm_context_t *context, int immediately );

/* used to maintain an external context relevant to the parent program */
pcomm_result_t pcomm_set_external_context( pcomm_context_t *context, void *external_context );
void *pcomm_get_external_context( pcomm_context_t *context );

/* set debug mode for pcomm */
void pcomm_set_debug( pcomm_context_t *context, int on );
int pcomm_get_debug( pcomm_context_t *context );

/* used to maintain an external file-descriptor-specific context relevant to
 * the parent program */
pcomm_result_t pcomm_set_external_fd_context( pcomm_context_t *context, pcomm_stream_t type,
                                            int fd, void *external_fd_context );
void *pcomm_get_external_fd_context( pcomm_context_t *context, pcomm_stream_t type, int fd );

/* sets the function to be called immediately before select is called */
pcomm_result_t pcomm_set_prepare_callback( pcomm_context_t *context,
                                           pcomm_callback_routine prepare_callback );

/* sets the function to be called immediately after select succeeds
 * (not called if select times out)
 */
pcomm_result_t pcomm_set_select_callback( pcomm_context_t *context,
                                          pcomm_callback_routine select_callback );

/* Function that is called whenever the select operation times out.
 * This allows certain operations to be performed at regular intervals while
 * pcomm retains controll of the overall program flow.
 */
pcomm_result_t pcomm_set_timeout_callback( pcomm_context_t *context, 
                                           pcomm_callback_routine timeout_callback );

/* changes the timeout for the select operation */
pcomm_result_t pcomm_set_timeout( pcomm_context_t *context, struct timeval *timeout );

/* sets the maximum number of bytes to read before returning control to 
 * the select
 */
pcomm_result_t pcomm_set_page_size( pcomm_context_t *context, size_t page_size );

/* add a file descriptor to the WRITE list for automatic I/O */
pcomm_result_t pcomm_add_write_fd( pcomm_context_t *context, int fd, 
                                 uint8_t *data, size_t length, 
                                 pcomm_callback_io io_callback, 
                                 pcomm_callback_ready close_callback );

/* add a file descriptor to the READ list for automatic I/O */
pcomm_result_t pcomm_add_read_fd(  pcomm_context_t *context, int fd, 
                                 pcomm_callback_io io_callback, 
                                 pcomm_callback_ready close_callback );

/* add a file descriptor to the ERROR list for automatic I/O */
pcomm_result_t pcomm_add_error_fd( pcomm_context_t *context, int fd, 
                                 pcomm_callback_io io_callback, 
                                 pcomm_callback_ready close_callback );

/* add a file descriptor to the WRITE list for alert */
pcomm_result_t pcomm_monitor_write_fd( pcomm_context_t *context, int fd,
                                       pcomm_callback_ready ready_callback );

/* add a file descriptor to the READ list for alert */
pcomm_result_t pcomm_monitor_read_fd( pcomm_context_t *context, int fd,
                                      pcomm_callback_ready ready_callback );

/* add a file descriptor to the ERROR list for alert */
pcomm_result_t pcomm_monitor_error_fd( pcomm_context_t *context, int fd,
                                       pcomm_callback_ready ready_callback );

pcomm_result_t pcomm_remove_read_fd(  pcomm_context_t *context, int fd );
pcomm_result_t pcomm_remove_write_fd( pcomm_context_t *context, int fd );
pcomm_result_t pcomm_remove_error_fd( pcomm_context_t *context, int fd );

/* convert a pcomm_result_t value into its string representation */
const char* pcomm_strresult(pcomm_result_t result);

#endif
