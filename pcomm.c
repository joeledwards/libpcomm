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
#include "pcomm.h"

int _list_aid_seeker( const void *element, const void *indicator )
{
    if ( element && indicator && 
         (((pcomm_fd_t *)element)->file_descriptor == *((int *)indicator)) ) {
        return 1;
    }
    return 0;
}

int _list_aid_comparator( const void *a, const void *b )
{
    if ( a && b ) {
        return ((pcomm_fd_t *)b)->file_descriptor - ((pcomm_fd_t *)a)->file_descriptor;
    }
    else if ( a && !b ) 
        return -1;
    else if ( !a && b )
        return 1;
    return 0;
}

pcomm_result_t _pcomm_make_fd_list( list_t *list, int **fd_list, size_t *length ) {
    pcomm_result_t result = PCOMM_SUCCESS;
    pcomm_fd_t *fd_context = NULL;
    size_t list_len = 0;
    int *fds = NULL;

    if ( !list ) {
        result = PCOMM_NULL_LIST;
    } else if ( !fd_list ) {
        result = PCOMM_NULL_FD;
    } else {
        list_len = list_size(list);
        if ( !(*fd_list = (int *)malloc(list_len * sizeof(int))) ) {
            result = PCOMM_OUT_OF_MEMORY;
        } else {
            fds = *fd_list;
            list_iterator_stop(list);
            list_iterator_start(list);
            *length = 0;
            while ( list_iterator_hasnext(list) ) {
                fd_context = list_iterator_next(list);
                if (fd_context) {
                    if ( list_len <= *length ) {
                        result = PCOMM_LIST_BAD_SIZE;
                        free(*fd_list);
                        break;
                    } else {
                        *fds = fd_context->file_descriptor;
                        fds++;
                        (*length)++;
                    }
                }
            }
            list_iterator_stop(list);
        }
    }

    return result;
}

pcomm_result_t _pcomm_add_input_fd( list_t *list, int fd, int check_only,
                                  pcomm_callback_ready ready_callback,
                                  pcomm_callback_io io_callback,
                                  pcomm_callback_ready close_callback ) 
{
    pcomm_fd_t *fd_context = NULL;
    pcomm_result_t result = PCOMM_SUCCESS;
    
    if ( fd < 0 ) {
        result = PCOMM_FD_NEGATIVE;
    } else if ( !list ) {
        result = PCOMM_NULL_LIST;
    } else if ( !(fd_context = calloc( sizeof(pcomm_fd_t), 1 )) ) {
        result = PCOMM_OUT_OF_MEMORY;
    } else {
        fd_context->file_descriptor = fd;
        fd_context->ready_callback = ready_callback;
        fd_context->io_callback = io_callback;
        fd_context->close_callback = close_callback;
        fd_context->buffer   = NULL;
        fd_context->length   = 0;
        fd_context->used     = 0;
        fd_context->offset   = 0;
        fd_context->last_read_empty = 0;
        fd_context->check_only = check_only;

        list_append( list, fd_context );
    }
    
    return result;
}

pcomm_result_t _pcomm_add_output_fd( list_t *list, int fd, int check_only,
                                   uint8_t *data, size_t length, 
                                   pcomm_callback_ready ready_callback,
                                   pcomm_callback_io io_callback,
                                   pcomm_callback_ready close_callback ) 
{
    pcomm_fd_t *fd_context = NULL;
    pcomm_result_t result = PCOMM_SUCCESS;
    uint8_t *new_buffer = NULL;

    if ( fd < 0 ) {
        result = PCOMM_FD_NEGATIVE;
    } else if ( !list ) {
        result = PCOMM_NULL_LIST;
    } else {
        // Try to locate an existing context for this descriptor
        if ( (fd_context = (pcomm_fd_t *)list_seek( list, &fd )) ) {
            if ( !fd_context ) {
                result = PCOMM_NULL_FD;
            } 
            // Check if we are managing I/O
            else if ( !fd_context->check_only ) {
                if ( !fd_context->buffer ) {
                    result = PCOMM_NULL_BUFFER;
                } else {
                    if ( !data || !length ) {
                        result = PCOMM_NO_DATA_FOR_WRITE;
                    } else if ( !(new_buffer = (uint8_t *)realloc(fd_context->buffer, fd_context->used + length)) ) {
                        result = PCOMM_OUT_OF_MEMORY;
                    } else {
                        fd_context->buffer = new_buffer;
                        memcpy( fd_context->buffer + fd_context->used, data, length );
                        fd_context->length = fd_context->used + length;
                        fd_context->used = fd_context->used + length;
                    }
                }
            }
        // If an existing context could not be located, try to create a new one
        } else if ( !(fd_context = calloc( sizeof(pcomm_fd_t), 1 )) ) {
            result = PCOMM_OUT_OF_MEMORY;
        // Initialize the new context if it was successfully created
        } else {
            fd_context->file_descriptor = fd;
            fd_context->ready_callback = ready_callback;
            fd_context->io_callback = io_callback;
            fd_context->close_callback = close_callback;
            fd_context->buffer   = NULL;
            fd_context->length   = 0;
            fd_context->used     = 0;
            fd_context->offset   = 0;
            fd_context->check_only = check_only;

            // Check if we are handling I/O
            if (!fd_context->check_only) {
                if ( !data || !length ) {
                    result = PCOMM_NO_DATA_FOR_WRITE;
                } else if ( !(fd_context->buffer = (uint8_t *)malloc(length)) ) {
                    result = PCOMM_OUT_OF_MEMORY;
                    free(fd_context);
                    fd_context = NULL;
                } else {
                    memcpy( fd_context->buffer, data, length );
                    fd_context->length = length;
                    fd_context->used = length;
                }
            }
            if (result == PCOMM_SUCCESS) {
                if ( list_append( list, fd_context ) < 0 ) {
                    fprintf(stderr, "Failed to add output descriptor to list.\n");
                }
            }
        }
    }
    
    return result;
}

pcomm_fd_t *_pcomm_get_fd( list_t *list, int fd ) 
{
    pcomm_fd_t *fd_context = NULL;

    if ( (fd >= 0) && (list) ) {
        fd_context = (pcomm_fd_t *)list_seek( list, &fd );
    } 

    return fd_context;
}

pcomm_result_t _pcomm_remove_fd( list_t *list, int fd ) 
{
    pcomm_result_t result = PCOMM_SUCCESS;
    int index = -1;
    int delete_result = -1;
    pcomm_fd_t *fd_context;

    if ( !list ) {
        result = PCOMM_NULL_LIST;
    } else if ( fd < 0 ) {
        result = PCOMM_FD_NEGATIVE;
    } else if ( ! (fd_context = _pcomm_get_fd(list, fd)) ) {
        result = PCOMM_FD_NOT_FOUND;
    } else if ( (index = list_locate(list, fd_context)) < 0 ) {
        result = PCOMM_INDEX_NOT_FOUND;
    } else if ( (delete_result = list_delete_at(list, (unsigned int)index)) < 0 ) {
        result = PCOMM_LIST_REMOVE_FAILED;
    } else {
        free(fd_context);
    }
    
    return result;
}

pcomm_result_t _pcomm_remove_fd_type( pcomm_context_t *context, int fd, pcomm_stream_t type )
{
    pcomm_result_t result = PCOMM_SUCCESS;
    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        if (type == PCOMM_STREAM_WRITE) {
            result = _pcomm_remove_fd(&context->write_fds, fd);
        } else if (type == PCOMM_STREAM_READ) {
            result = _pcomm_remove_fd(&context->read_fds, fd);
        } else if (type == PCOMM_STREAM_ERROR) {
            result = _pcomm_remove_fd(&context->error_fds, fd);
        } else {
            result = PCOMM_INVALID_STREAM_TYPE;
        }
    }
    return result;
}

pcomm_result_t _pcomm_empty_list( list_t *list ) {
    pcomm_result_t result = PCOMM_SUCCESS;
    pcomm_fd_t *fd_context = NULL;

    if ( !list ) {
        result = PCOMM_NULL_LIST;
    } else {
        list_iterator_stop(list); 
        list_iterator_start(list); 
        while ( list_iterator_hasnext(list) ) {
            fd_context = list_iterator_next(list);
            if ( fd_context ) {
                if ( fd_context->buffer ) {
                    free( fd_context->buffer );
                }
                free( fd_context );
            }
        }
        list_iterator_stop(list); 
        list_clear(list);
    }

    return result;
}

int _pcomm_writes_buffered( pcomm_context_t *context ) { 
    int result = 0;
    /*
    pcomm_fd_t *fd_context = NULL;
    */

    if ( context ) {
        /* This assumes that all writes are being handled correctly */
        if ( !list_empty(&context->write_fds) ) {
            result = 1;
        }
        /*
        list_iterator_stop(&context->write_fds); 
        list_iterator_start(&context->write_fds); 
        while ( list_iterator_hasnext(&context->write_fds) ) {
            fd_context = list_iterator_next(&context->write_fds);
            if ( fd_context ) {
                if ( fd_context->buffer && 
                     fd_context->used   &&
                    (fd_context->offset <= fd_used) ) {
                    result = 1;
                    break;
                }
            }
        }
        list_iterator_stop(&context->write_fds); 
        */
    }

    return result;
}

int _pcomm_populate_set( list_t *list, fd_set *set ) 
{
    int max_fd = -1;
    int tmp_fd = -1;
    pcomm_fd_t *fd_context = NULL; 

    FD_ZERO( set );
    if ( list && set ) {
        list_iterator_stop(list); 
        list_iterator_start(list); 
        while ( list_iterator_hasnext(list) ) {
            fd_context = list_iterator_next(list);
            if ( fd_context ) {
                tmp_fd = fd_context->file_descriptor;
                if ( tmp_fd >= 0 ) {
                    if (tmp_fd > max_fd) {
                        max_fd = tmp_fd;
                    }
                    FD_SET( tmp_fd, set );
                }
            }
        }
        list_iterator_stop(list); 
    }
    return max_fd;
}

pcomm_result_t _pcomm_clean_read_buffer( pcomm_fd_t *fd_context )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !fd_context ) {
        result = PCOMM_NULL_CONTEXT;
    } else {
        if ( fd_context->buffer ) {
            free( fd_context->buffer );
        }
        fd_context->buffer = NULL;
        fd_context->length = 0;
        fd_context->used   = 0;
    }

    return result;
}

pcomm_result_t _read_fd( pcomm_fd_t *fd_context, size_t page_size )
{
    pcomm_result_t result = PCOMM_SUCCESS;
    uint8_t *new_buffer = NULL;
    uint8_t *buffer = NULL;
    size_t read_count = 0;

    if ( !fd_context) {
        result = PCOMM_NULL_CONTEXT;
    } else if ( !(buffer = (uint8_t *)malloc(page_size)) ) {
        result = PCOMM_OUT_OF_MEMORY;
    } else if ( (read_count = read(fd_context->file_descriptor, buffer, page_size)) <= 0 ) {
        result = PCOMM_NO_DATA_FROM_READ;
    } else if ( !(new_buffer = realloc( fd_context->buffer, fd_context->used + read_count )) ) {
        result = PCOMM_OUT_OF_MEMORY;
    } else {
        fd_context->buffer = new_buffer;
        memcpy( fd_context->buffer + fd_context->used, buffer, read_count );
        fd_context->length = fd_context->used + read_count;
        fd_context->used = fd_context->used + read_count;
    }

    return result;
}

pcomm_result_t _write_fd( pcomm_fd_t *fd_context )
{
    pcomm_result_t result = PCOMM_SUCCESS;
    uint8_t *new_buffer = NULL;
    size_t write_count = 0;

    if ( !fd_context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if ( !fd_context->buffer ) { 
        result = PCOMM_NULL_BUFFER;
    } else if ( !fd_context->used ) {
        result = PCOMM_NO_DATA_FOR_WRITE;
    } else if ( !(write_count = write(fd_context->file_descriptor, fd_context->buffer, fd_context->used)) ) {
        result = PCOMM_FD_WRITE_FAILED;
    } else {
        /* Update the number of bytes left, if empty, null out buffer */
        if ( write_count == fd_context->used ) {
            free(fd_context->buffer);
            fd_context->buffer = NULL;
            fd_context->length = 0;
            fd_context->used = 0;
        } else {
            fd_context->used -= write_count;
            memmove(fd_context->buffer, fd_context->buffer + write_count, fd_context->used);
            if ( (new_buffer = realloc(fd_context->buffer, fd_context->used)) ) {
                fd_context->buffer = new_buffer;
                fd_context->length = fd_context->used;
            }
        }
    }

    return result;
}

/* check how many bytes are buffered on a socket */
long _bytes_on_socket(int socket)
{
    size_t bytes_available = 0;
    if ( ioctl(socket, FIONREAD, (char *)&bytes_available) < 0 ) {
        return -1;
    }
    return((long)bytes_available);
}


/* Manage I/O and callbacks for all selected file descriptors */
void _process_selected_fds( pcomm_context_t *context,
                            pcomm_stream_t stream,
                            fd_set        *set_ptr) {
    pcomm_fd_t *fd_context;
    list_t *stream_fds;

    int io_result;
    int *fds = NULL;
    size_t fds_len = 0;

    int i = 0;

    if (!context) {
        return;
    }
    if (context->exit_now) {
        return;
    }

    switch (stream) {
        case PCOMM_STREAM_WRITE:
            stream_fds = &context->write_fds;
            break;
        case PCOMM_STREAM_READ:
            stream_fds = &context->read_fds;
            break;
        case PCOMM_STREAM_ERROR:
            stream_fds = &context->error_fds;
            break;
    }

    if ( !_pcomm_make_fd_list(stream_fds, &fds, &fds_len) ) {
        for ( i=0; (i<fds_len) && (!context->exit_now); i++ ) {
            if ( FD_ISSET(fds[i], set_ptr) ) {
                if ( (fd_context = (pcomm_fd_t *)list_seek(stream_fds, &fds[i])) ) {
                    // Check if we are only notifying that fd is ready
                    if (fd_context->check_only) {
                        if (context->debug) {
                            fprintf(stderr, "File descriptor %d is ready\n", fd_context->file_descriptor);
                        }
                        if (fd_context->ready_callback) {
                            fd_context->ready_callback( context,
                                                        fd_context->file_descriptor );
                        }
                    }
                    // Otherwise we try to perform I/O on this fd
                    else {
                        if (stream == PCOMM_STREAM_WRITE) {
                            io_result = _write_fd( fd_context );
                            if ( fd_context->io_callback ) {
                                fd_context->io_callback( context, 
                                                      fd_context->file_descriptor,
                                                      NULL, 0);
                            }
                            if ( !fd_context->buffer || !fd_context->used ) {
                                _pcomm_remove_fd( &context->write_fds, 
                                                  fd_context->file_descriptor );
                                if (fd_context->close_callback) {
                                    fd_context->close_callback(context,
                                                               fd_context->file_descriptor);
                                }
                                free(fd_context);
                            }
                        }
                        else {
                            io_result = _read_fd( fd_context, context->page_size );
                            if (!io_result) {
                                fd_context->last_read_empty = 0;
                                if (fd_context->io_callback ) {
                                    fd_context->io_callback( context, 
                                                          fd_context->file_descriptor, 
                                                          fd_context->buffer, 
                                                          fd_context->used );
                                    _pcomm_clean_read_buffer( fd_context );
                                }
                            } else if (io_result == PCOMM_NO_DATA_FROM_READ) {
                                if (fd_context->last_read_empty) {
                                    pcomm_remove_read_fd(context, fd_context->file_descriptor);
                                    if (fd_context->close_callback) {
                                        fd_context->close_callback(context,
                                                                   fd_context->file_descriptor);
                                    }
                                } else {
                                    fd_context->last_read_empty = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        free(fds);
        fds = NULL;
        fds_len = 0;
    }
}

/* The real magic happens here */
pcomm_result_t _pcomm_loop( pcomm_context_t *context ) {
    pcomm_result_t result = PCOMM_SUCCESS;
    int read_max, write_max, error_max;
    int max_fd;
    int num_fds;
    struct timeval timeout;

    fd_set read_set;
    fd_set write_set;
    fd_set error_set;

    fd_set *read_set_ptr;
    fd_set *write_set_ptr;
    fd_set *error_set_ptr;

    if ( !context ) {
        fprintf( stderr, "Context null!\n" );
        return PCOMM_NULL_CONTEXT;
    }

    while (!context->exit_now)
 // PCOMM LOOP
    {
        if (context->exit_request  && !_pcomm_writes_buffered(context)) {
            context->exit_now = 1;
            continue;
        }

        // call the preparation routine if supplied
        if (context->prepare_callback) {
            context->prepare_callback(context);
        }

        if (context->exit_now) {
            continue;
        }

        // populate write fds
        max_fd = -1;
        write_max = _pcomm_populate_set( &context->write_fds, &write_set );
        max_fd = (write_max > max_fd) ? write_max : max_fd;

        // do not process reads if we are trying to exit cleanly!
        if (!context->exit_request) {
            // populate read fds
            read_max = _pcomm_populate_set( &context->read_fds, &read_set );
            max_fd = (read_max > max_fd) ? read_max : max_fd;

            // populate error fds
            error_max = _pcomm_populate_set( &context->error_fds, &error_set );
            max_fd = (error_max > max_fd) ? error_max : max_fd;
        }
        else {
            read_max = -1;
            error_max = -1;
        }

        if ( max_fd < 0 ) {
            result = PCOMM_FD_NOT_FOUND;
            context->exit_now = 1;
            continue;
        }

        if (context->debug) {
            fprintf( stderr, "pcomm: max_fd = %d\n", max_fd );
            fprintf( stderr, "pcomm:   write_fd = %d\n", write_max );
            fprintf( stderr, "pcomm:   read_fd  = %d\n", read_max );
            fprintf( stderr, "pcomm:   error_fd = %d\n", error_max );
        }

        write_set_ptr = (write_max >= 0) ? &write_set : NULL;
        read_set_ptr  = (read_max  >= 0) ? &read_set  : NULL;
        error_set_ptr = (error_max >= 0) ? &error_set : NULL;

        timeout.tv_sec = context->timeout.tv_sec;
        timeout.tv_usec = context->timeout.tv_usec;

        num_fds = select( max_fd + 1, read_set_ptr, write_set_ptr, error_set_ptr, &timeout );
        if ( num_fds < 0 ) {
            // before potentially resetting, check for exit
            if (context->exit_now) {
                continue;
            }
            context->exit_now = 1;
            switch (errno) {
                case EBADF:
                    fprintf( stderr, "Bad file descriptor\n" );
                    break;
                case EINTR:
                    fprintf( stderr, "Caught interrupt signal\n" );
                    break;
                case EINVAL:
                    fprintf( stderr, "Invalid timeout or max fd\n" );
                    break;
                case ENOMEM:
                    fprintf( stderr, "Out of memory\n" );
                    break;
                default:
                    fprintf( stderr, "Unknown error with select\n" );
                    context->exit_now = 0;
            }
        } else if ( num_fds == 0 ) {
            if (context->exit_now) {
                continue;
            }
            if (context->debug) {
                fprintf( stderr, "pcomm: timeout occurred (no descriptor selected)\n" );
            }
            else if (context->timeout_callback) {
                context->timeout_callback(context);
            }
        } else {
            // call the post select routine if supplied
            if (context->select_callback) {
                if (context->debug) {
                    fprintf( stderr, "pcomm: calling select routine\n" );
                }
                context->select_callback(context);
            }
            if (context->exit_now) {
                continue;
            }

            if (context->debug) {
                fprintf( stderr, "pcomm: processing file descriptors\n" );
            }
            // This order is intential.
            _process_selected_fds(context, PCOMM_STREAM_ERROR, error_set_ptr);
            _process_selected_fds(context, PCOMM_STREAM_WRITE, write_set_ptr);
            _process_selected_fds(context, PCOMM_STREAM_READ,  read_set_ptr);
        }
 // PCOMM LOOP
    }

    return result;
}

pcomm_result_t pcomm_init( pcomm_context_t *context )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) {
        result = PCOMM_NULL_CONTEXT;
    }
    else if ( list_init( &context->read_fds  ) || 
              list_init( &context->write_fds ) || 
              list_init( &context->error_fds ) ) {
        result = PCOMM_INIT_FAILED;
    } else {
        list_attributes_seeker( &context->read_fds,  _list_aid_seeker );
        list_attributes_seeker( &context->write_fds, _list_aid_seeker );
        list_attributes_seeker( &context->error_fds, _list_aid_seeker );
        list_attributes_comparator( &context->read_fds,  _list_aid_comparator );
        list_attributes_comparator( &context->write_fds, _list_aid_comparator );
        list_attributes_comparator( &context->error_fds, _list_aid_comparator );
        context->page_size = PCOMM_PAGE_SIZE;
        context->prepare_callback = NULL;
        context->select_callback = NULL;
        context->timeout_callback = NULL;
        context->timeout.tv_sec = 0;
        context->timeout.tv_usec = 0;
        context->initialized = 1;
        context->debug = 0;
        context->exit_request = 0;
        context->exit_now = 0;
        context->external_context = NULL;
    }

    return result;
}

void pcomm_set_debug( pcomm_context_t *context, int debug )
{
    if (context) {
        context->debug = debug;
    }
}

int pcomm_get_debug( pcomm_context_t *context )
{
    if (context) {
        return context->debug;
    }
    return 0;
}

pcomm_result_t pcomm_destroy( pcomm_context_t *context )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else {
        if (context->initialized) {
            context->initialized = 0;
            context->external_context = NULL;
            _pcomm_empty_list( &context->read_fds );
            _pcomm_empty_list( &context->write_fds );
            _pcomm_empty_list( &context->error_fds );
            list_destroy( &context->read_fds );
            list_destroy( &context->write_fds );
            list_destroy( &context->error_fds );
        }
    }
    return result;
}

pcomm_result_t pcomm_main( pcomm_context_t *context ) 
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request || context->exit_now) {
        result = PCOMM_EXITING;
    } else {
        result = _pcomm_loop( context );
    }

    return result;
}

pcomm_result_t pcomm_stop( pcomm_context_t *context, int immediately ) 
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else {
        context->exit_request = 1;
        if ( immediately ) {
            context->exit_now = 1;
        }
    }

    return result;
}

pcomm_result_t pcomm_set_external_context( pcomm_context_t *context, void *external_context ) {
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->external_context = external_context;
    }

    return result;
}

void *pcomm_get_external_context( pcomm_context_t *context ) {
    void *external_context = NULL;
    if (context && context->initialized) {
        external_context = context->external_context;
    }
    return external_context;
}

pcomm_result_t pcomm_set_external_fd_context( pcomm_context_t *context, pcomm_stream_t type,
                                   int fd, void *external_fd_context ) {
    pcomm_result_t result = PCOMM_SUCCESS;
    pcomm_fd_t *fd_context = NULL;
    list_t *list = NULL;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        switch(type) {
            case PCOMM_STREAM_WRITE:
                list = &context->write_fds;
                break;
            case PCOMM_STREAM_READ:
                list = &context->read_fds;
                break;
            case PCOMM_STREAM_ERROR:
                list = &context->error_fds;
                break;
            default:
                result = PCOMM_INVALID_STREAM_TYPE;
        }
        if (result == PCOMM_SUCCESS) {
            if ( !list ) {
                result = PCOMM_NULL_LIST;
            } else if ( fd < 0 ) {
                result = PCOMM_FD_NEGATIVE;
            } else if ( ! (fd_context = _pcomm_get_fd(list, fd)) ) {
                result = PCOMM_FD_NOT_FOUND;
            } else {
                fd_context->external_context = external_fd_context;
            }
        }
    }

    return result;
}

void *pcomm_get_external_fd_context( pcomm_context_t *context, pcomm_stream_t type, int fd ) {
    void *external_fd_context = NULL;
    list_t *list = NULL;
    pcomm_fd_t *fd_context = NULL;

    if (context && context->initialized) {
        switch(type) {
            case PCOMM_STREAM_WRITE:
                list = &context->write_fds;
                break;
            case PCOMM_STREAM_READ:
                list = &context->read_fds;
                break;
            case PCOMM_STREAM_ERROR:
                list = &context->error_fds;
                break;
        }
        
        if (list && (fd >= 0) && (fd_context = _pcomm_get_fd(list, fd))) {
            external_fd_context = fd_context->external_context;
        }
    }
    return external_fd_context;
}

pcomm_result_t pcomm_set_page_size( pcomm_context_t *context, size_t page_size )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->page_size = page_size;
    }

    return result;
}

pcomm_result_t pcomm_set_timeout( pcomm_context_t *context, struct timeval *timeout )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->timeout.tv_sec = timeout->tv_sec;
        context->timeout.tv_usec = timeout->tv_usec;
    }

    return result;
}

pcomm_result_t pcomm_set_timeout_callback( pcomm_context_t *context, 
                                           pcomm_callback_routine timeout_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->timeout_callback = timeout_callback;
    }

    return result;
}

pcomm_result_t pcomm_set_prepare_callback( pcomm_context_t *context,
                                           pcomm_callback_routine prepare_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->prepare_callback = prepare_callback;
    }

    return result;
}

pcomm_result_t pcomm_set_select_callback( pcomm_context_t *context,
                                          pcomm_callback_routine select_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if (!context) {
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else {
        context->select_callback = select_callback;
    }

    return result;
}

pcomm_result_t pcomm_add_write_fd( pcomm_context_t *context, int fd, 
                                 uint8_t *data, size_t length, 
                                 pcomm_callback_io io_callback, 
                                 pcomm_callback_ready close_callback )
{ 
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else if (!data || !length) {
        result = PCOMM_NO_DATA_FOR_WRITE;
    } else {
        result = _pcomm_add_output_fd( &context->write_fds, fd,
                                       0    /*check_only*/,
                                       data,
                                       length, 
                                       NULL /*ready_callback*/, 
                                       io_callback,
                                       close_callback ); 
    }

    return result;
}
pcomm_result_t pcomm_monitor_write_fd( pcomm_context_t *context, int fd,
                                       pcomm_callback_ready ready_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else {
        result = _pcomm_add_output_fd( &context->write_fds, fd,
                                       1    /*check_only*/,
                                       NULL /*data*/,
                                       0    /*length*/,
                                       ready_callback,
                                       NULL /*io_callback*/,
                                       NULL /*close_callback*/ ); 
    }

    return result;
}


pcomm_result_t pcomm_add_read_fd( pcomm_context_t *context, int fd, 
                                pcomm_callback_io io_callback, 
                                pcomm_callback_ready close_callback ) 
{ 
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else if ( !io_callback ) {
        result = PCOMM_NULL_CALLBACK;
    } else {
        result = _pcomm_add_input_fd( &context->read_fds, fd,
                                      0    /*check_only*/,
                                      NULL /*ready_callback*/,
                                      io_callback,
                                      close_callback ); 
    }

    return result;
}
pcomm_result_t pcomm_monitor_read_fd( pcomm_context_t *context, int fd,
                                      pcomm_callback_ready ready_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else if (!ready_callback) {
        result = PCOMM_NULL_CALLBACK;
    } else {
        result = _pcomm_add_input_fd( &context->read_fds, fd,
                                      1    /*check_only*/,
                                      ready_callback,
                                      NULL /*io_callback*/,
                                      NULL /*close_callback*/ ); 
    }

    return result;
}


pcomm_result_t pcomm_add_error_fd( pcomm_context_t *context, int fd, 
                                 pcomm_callback_io io_callback, 
                                 pcomm_callback_ready close_callback )
{ 
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else if ( !io_callback ) {
        result = PCOMM_NULL_CALLBACK;
    } else {
        result = _pcomm_add_input_fd( &context->error_fds, fd,
                                      0    /*check_only*/,
                                      NULL /*ready_callback*/,
                                      io_callback,
                                      close_callback ); 
    }

    return result;
}
pcomm_result_t pcomm_monitor_error_fd( pcomm_context_t *context, int fd,
                                       pcomm_callback_ready ready_callback )
{
    pcomm_result_t result = PCOMM_SUCCESS;

    if ( !context ) { 
        result = PCOMM_NULL_CONTEXT;
    } else if (!context->initialized) {
        result = PCOMM_UNINITIALIZED_CONTEXT;
    } else if (context->exit_request) {
        result = PCOMM_EXITING;
    } else if (!ready_callback) {
        result = PCOMM_NULL_CALLBACK;
    } else {
        result = _pcomm_add_input_fd( &context->error_fds, fd,
                                      1    /*check_only*/,
                                      ready_callback,
                                      NULL /*io_callback*/,
                                      NULL /*close_callback*/ ); 
    }

    return result;
}

pcomm_result_t pcomm_remove_write_fd( pcomm_context_t *context, int fd )
{ 
    pcomm_result_t result = PCOMM_SUCCESS;
    result = _pcomm_remove_fd_type(context, fd, PCOMM_STREAM_WRITE);
    return result;
}

pcomm_result_t pcomm_remove_read_fd( pcomm_context_t *context, int fd )
{ 
    pcomm_result_t result = PCOMM_SUCCESS;
    result = _pcomm_remove_fd_type(context, fd, PCOMM_STREAM_READ);
    return result;
}

pcomm_result_t pcomm_remove_error_fd( pcomm_context_t *context, int fd )
{ 
    pcomm_result_t result = PCOMM_SUCCESS;
    result = _pcomm_remove_fd_type(context, fd, PCOMM_STREAM_ERROR);
    return result;
}

const char* pcomm_strresult(pcomm_result_t result)
{
    switch (result) {
        case PCOMM_SUCCESS:
            return "pcomm: operation succeeded";
        case PCOMM_INIT_FAILED:
            return "pcomm: init failed";
        case PCOMM_DESTROY_FAILED:
            return "pcomm: destroy failed";
        case PCOMM_FD_OPEN_FAILED:
            return "pcomm: fd open failed";
        case PCOMM_FD_READ_FAILED:
            return "pcomm: fd read failed";
        case PCOMM_FD_WRITE_FAILED:
            return "pcomm: fd write failed";
        case PCOMM_FD_CLOSE_FAILED:
            return "pcomm: fd close failed";
        case PCOMM_FD_NOT_FOUND:
            return "pcomm: fd not found";
        case PCOMM_FD_NEGATIVE:
            return "pcomm: fd is negative";
        case PCOMM_FD_EOF_REACHED:
            return "pcomm: fd reached EOF";
        case PCOMM_INDEX_NOT_FOUND:
            return "pcomm: list index not found";
        case PCOMM_LIST_REMOVE_FAILED:
            return "pcomm: list removal failed";
        case PCOMM_LIST_BAD_SIZE:
            return "pcomm: bad list size";
        case PCOMM_NO_DATA_FROM_READ:
            return "pcomm: no data read";
        case PCOMM_NO_DATA_FOR_WRITE:
            return "pcomm: no data written";
        case PCOMM_OUT_OF_MEMORY:
            return "pcomm: out of memory";
        case PCOMM_TERMINATED_BY_ERROR:
            return "pcomm: terminated by error";
        case PCOMM_UNINITIALIZED_CONTEXT:
            return "pcomm: not initialized";
        case PCOMM_NULL_CONTEXT:
            return "pcomm: null context";
        case PCOMM_NULL_CALLBACK:
            return "pcomm: null callback";
        case PCOMM_NULL_LIST:
            return "pcomm: null I/O list";
        case PCOMM_NULL_FD:
            return "pcomm: null fd context";
        case PCOMM_NULL_BUFFER:
            return "pcomm: null buffer";
        case PCOMM_DUPLICATE_FD:
            return "pcomm: duplicate fd";
        case PCOMM_INVALID_STREAM_TYPE:
            return "pcomm: invalid stream type";
        case PCOMM_EXITING:
            return "pcomm: exiting";
    }
    return "Unrecognized";
}

