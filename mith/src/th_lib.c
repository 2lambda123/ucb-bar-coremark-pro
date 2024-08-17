/*
(C) 2014 EEMBC(R).  All rights reserved.

All EEMBC Benchmark Software are products of EEMBC
and are provided under the terms of the EEMBC Benchmark License Agreements.
The EEMBC Benchmark Software are proprietary intellectual properties of EEMBC
and its Members and is protected under all applicable laws, including all
applicable copyright laws. If you received this EEMBC Benchmark Software without
having a currently effective EEMBC Benchmark License Agreement, you must
discontinue use. Please refer to LICENSE.md for the specific license agreement
that pertains to this Benchmark Software.
*/

/*==============================================================================
 * File: mith/src/th_lib.c
 *
 *   Description: TH_Lite Library Routines
 *
 *  EEMBC : EEMBC Technical Advisory Group (TechTAG)
 *
 */

/*------------------------------------------------------------------------------
 *
 *
 *------------------------------------------------------------------------------
 * Other Copyright Notice (if any):
 *
 * For conditions of distribution and use, see the accompanying README file.
 * ===========================================================================*/

/* initial version of mith, rely on stdio to provide thread safe file io */
#include "th_cfg.h"
#include "th_encode.h"
#include "th_math.h"
#include <stdio.h>
#include <stdlib.h>
#define FILE_TYPE_DEFINED
#include "al_file.h"
#include "th_al.h"
#include "th_file.h"
#include "th_lib.h"
/** todo : Drop these includes when we have implemented thread safe versions of
 * required. */
#include <string.h>
#ifndef TH_SAFE_MALLOC
#define TH_SAFE_MALLOC 1
#endif
#if REPORT_THMALLOC_STATS
#include "al_smp.h"
al_mutex_t th_malloc_mutex;
size_t th_malloc_total = 0;
size_t th_malloc_max = 0;
ee_intlist *th_malloc_sizes = NULL;
void init_thmalloc_stats();
#endif

e_u32 pgo_training_run = 0;

/*#define THDEBUG 3*/
/*------------------------------------------------------------------------------
 * function: th_timer_available
 *
 * Description:
 *			used to determine if the duration timer is available.
 *
 * Returns:
 *			TRUE if the target supports the duration timer.
 *          FALSE if not
 * ---------------------------------------------------------------------------*/

int th_timer_available(void) { return TARGET_TIMER_AVAIL; }

/*------------------------------------------------------------------------------
 * function: th_timer_is_intrusive
 *
 * Description:
 *			used to determine if the timer function is intrusive.
 *
 *          Intrusive usually means that operating and maintaining the timer
 *          has a run time overhead.  For example, using a 10ms interrupt
 *          to incriment a timer value is intrusive because the interrupt
 *          service routine takes CPU time.
 *
 *          If an intrusive target timer is used to measure benchmarks, then
 *          its effect must be measured and taken into account.
 *
 *          Some target timers may not be intrusive.  For example, a target
 *          with a built in real time clock can measure time without an
 *          interrupt service routine.
 *
 *          Another way to build a non-intrusive timer is to cascade
 *          counter/timer circuits to get very large divisors.  For example,
 *          with a 20mhz system clock two 16bit timers could be cascaded to
 *          measure durations up to 3 minutes and 34 seconds before rolling
 *          over.
 *
 * Note:
 *			If the timer is not avaialable, then this function
 *returns FALSE.
 *
 *
 * Returns:
 *			TRUE if the target's duration timer is intrusive
 *          FALSE if it is not
 * ---------------------------------------------------------------------------*/

int th_timer_is_intrusive(void) { return TARGET_TIMER_INTRUSIVE; }

/*------------------------------------------------------------------------------
 * function: th_ticks_per_sec
 *
 * Description:
 *			used to determine the number of timer ticks per second.
 *
 * Returns:
 *			The number of timer ticks per second returned by
 *th_stop_timer()
 * ---------------------------------------------------------------------------*/

size_t th_ticks_per_sec(void) { return al_ticks_per_sec(); }

/*------------------------------------------------------------------------------
 * function: th_tick_granularity
 *
 * Description:
 *			used to determine the granularity of timer ticks.
 *
 *          For example, the value returned by th_stop_timer() may be
 *          in milliseconds. In this case, th_ticks_pers_sec() would
 *          return 1000.  However, the timer interrupt may only occur
 *          once very 10ms.  So th_tick_granularity() would return 10.
 *
 *          However, on another system, th_ticks_sec() might return 10
 *          and th_tick_granularity() could return 1.  This means that each
 *          incriment of the value returned by th_stop_timer() is in 100ms
 *          intervals.
 *
 * Returns:
 *			the granularity of the value returned by th_stop_timer()
 * ---------------------------------------------------------------------------*/

size_t th_tick_granularity(void) { return al_tick_granularity(); }

/*------------------------------------------------------------------------------
 * Function: th_signal_now()
 *
 * Description:
 *	Get current time.
 *---------------------------------------------------------------------------*/
size_t th_signal_now(void) { return al_signal_now(); }

/*void th_signal_start( void ) {al_signal_start();}*/

/*------------------------------------------------------------------------------
 * Obsolete: th_signal_finished
 *
 * Description:
 *			Signals the host that the test is finished
 *			In MITH, the harness controls the timer.
 *
 *          This function is called to signal the host system that the
 *          currently executing test or benchmark is finished.  The host
 *          uses this signal to mark the stop time of the test and to
 *          measure the duration of the test.
 *
 * Returns:
 *			The duration of the test in 'ticks' as measured by the
 *target's timer (if one is available).  If the target does not have a timer, or
 *it is not supported by the TH, then this function returns TH_UNDEF_VALUE.
 *
 * Note:
 *			There are intentionally no parameters for this function.
 *It is designed to have very low overhead. It causes a short message to be sent
 *to the host indicating.  The results of the test are reported using another
 *function.
 * ---------------------------------------------------------------------------*/

/*e_u32 th_signal_finished( void ) {return al_signal_finished();}*/

/*------------------------------------------------------------------------------
 * function: th_exit
 *
 * Description:
 *		The benchmark code failed internally.
 *
 * Returns:
 *		Exits, no return.
 * Note:
 *      Add any signal code to aid debugging.
 * ----------------------------------------------------------------------------*/
/* coverity[+kill] */
void th_exit(int exit_code, const char *fmt, ...) {
  char buf[256];
  va_list args;
  // FILE *f=th_fopen("debug.txt","at");
  va_start(args, fmt);
  // th_fprintf( f, fmt, args );
  // th_fclose(f);
  th_vsprintf(buf, fmt, args);
  th_printf("%s", buf);
  va_end(args);
  al_exit(exit_code);
}

/** Function: th_getenv
        Description:
        Get Environment Variables

        Note:
        This function is not currently used.
*/
char *th_getenv(const char *key) { return al_getenv(key); }

/**
 * File I/O section, standard C parameters, but th_file.h is used instead of
 *stdio
 *
 * Note :
 *			These could be done with #defines, but in the future we
 *may want TH implementations
 */
/* Variables: std*
        For initial version of MITH, simple redirection.
*/
void *th_stdin;
void *th_stdout;
void *th_stderr;

void redirect_std_files() {
  th_stdin = stdin;
  th_stdout = stdout;
  th_stderr = stderr;
}

/* Function: th_fclose
        Close a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fclose(ee_FILE *fp) { return al_fclose(fp); }
/* Function: th_ferror
        Return last error associated with file.

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_ferror(ee_FILE *fp) { return al_ferror(fp); }
/* Function: th_feof
        Test for end of file.

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_feof(ee_FILE *fp) { return al_feof(fp); }
/* Function: th_clearerr
        Clear last error associated with file.

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
void th_clearerr(ee_FILE *fp) { al_clearerr(fp); }
/* Function: th_fileno
        Return handle associated with file pointer.

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fileno(ee_FILE *fp) { return al_fileno(fp); }
/* Function: th_fflush
        Flush to file.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fflush(ee_FILE *fp) { return al_fflush(fp); }
/* Function: th_fprintf
        Formatted print to a file

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fprintf(ee_FILE *fp, const char *format, ...) {
  int rv;
  va_list args;
  va_start(args, format);
  rv = al_vfprintf(fp, format, args);
  va_end(args);
  return rv;
}
/* Function: th_vfprintf
        Formatted print to a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_vfprintf(ee_FILE *fp, const char *format, va_list ap) {
  return al_vfprintf(fp, format, ap);
}
/* Function: th_fseek
        Change current location of file pointer

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fseek(ee_FILE *fp, long int offset, int whence) {
  return al_fseek(fp, offset, whence);
}
/* Function: th_ftell
        Report current position of file pointer

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
long th_ftell(ee_FILE *fp) { return al_ftell(fp); }

/* DIRENT routines, and helpers */
/* TODO: Initial version of MITH, no support for DIRENT!
char      *th_getcwd   (char *buffer, size_t size){return getcwd(buffer,size);}
char      *th_getwd    (char *buffer){return getwd(buffer);}
int        th_chdir    (const char *filename){return chdir(filename);}
ee_DIR    *th_opendir  (const char *dirname){return opendir(dirname);}
struct ee_dirent *th_readdir  (ee_DIR *dirstream){return readdir(dirstream);}
int        th_closedir (ee_DIR *dirstream){return closedir(dirstream);}
void       th_rewinddir(ee_DIR *dirstream){rewinddir(dirstream);}
*/
/* Function: th_stat
        Report status for a file name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_stat(const char *path, void *buf) { return al_stat(path, buf); }
/* Function: th_lstat
        Report status for a file name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_lstat(const char *path, void *buf) { return al_stat(path, buf); }
/* Function: th_fstat
        Report status for a file descriptor

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fstat(int fildes, void *buf) { return al_fstat(fildes, buf); }
/* Function: th_unlink
        Delete a file by name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_unlink(const char *filename) { return al_unlink(filename); }
/* Function: th_unlink
        Delete a file by name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_rename(const char *oldpath, const char *newpath) {
  return al_rename(oldpath, newpath);
}

/* Basic file I/O routines */

/* Function: th_putc
        Put a char in a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_putc(int c, ee_FILE *fp) { return al_putc(c, fp); }
/* Function: th_getc
        Gets a char from a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_getc(ee_FILE *fp) { return al_getc(fp); }
/* Function: th_ungetc
        Return a char to a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_ungetc(int ch, ee_FILE *file) { return al_ungetc(ch, file); }
/* Function: th_fputs
        Write a string to a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
int th_fputs(const char *s, ee_FILE *fp) { return al_fputs(s, fp); }
/* Function: th_fgets
        Reads a string from a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
char *th_fgets(char *string, int count, ee_FILE *file) {
  return al_fgets(string, count, file);
}
/* Function: th_fread
        Reads a buffer from a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
size_t th_fread(void *buf, size_t size, size_t count, ee_FILE *fp) {
  return al_fread(buf, size, count, fp);
}
/* Function: th_fwrite
        Writes a buffer to a file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
size_t th_fwrite(const void *buf, size_t size, size_t count, ee_FILE *fp) {
  return al_fwrite(buf, size, count, fp);
}
/* Function: th_fopen
        Open a file by name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
#ifndef EE_DEMO
ee_FILE *th_fopen(const char *filename, const char *mode) {
  return al_fopen(filename, mode);
}
#else
ee_FILE *th_fopen(const char *filename, const char *mode) {
  if (th_strstr(filename, "../data") != NULL)
    filename += 3;
  return al_fopen(filename, mode);
}
#endif
/* Function: th_fdopen
        Open a file by file descriptor

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
ee_FILE *th_fdopen(int fildes, const char *mode) {
  return al_fdopen(fildes, mode);
}
/* Function: th_fopen
        Reopen a file by name

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
ee_FILE *th_freopen(const char *filename, const char *mode, ee_FILE *fp) {
  return al_freopen(filename, mode, fp);
}
/* Function: th_tmpfile
        Create a temporary file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
ee_FILE *th_tmpfile(void) { return al_tmpfile(); }
/* Function: th_mktemp
        Create a temporary file

        Porting:
                May need to port this function, depending on tools support and
   specific benchmark use.

        Note:
                Redirected to stdio in initial version of MITH
*/
char *th_mktemp(char *templat) { return al_mktemp(templat); }

/* scanf input format conversion family (thal.c only sees vxxx functions) */
/* scan from console should not be needed in a benchmark context */
/* only from file or string in test harness */
/* TODO : Initial version of MITH relies on OS to provide thread safe scanf
functions

int 	th_scanf(const char *format, ...)
{
   int rv;
   va_list args;
   va_start( args, format );
   rv = th_vscanf( format, args );
   va_end( args );
   return rv;
}
int		th_vscanf(const char *format, va_list ap){return vscanf( format,
ap );} int		th_vsscanf(const char *str, const char *format, va_list
ap){return al_vsscanf( str,format, ap );}
*/

/* Function: th_sscanf
        Create a temporary file

        Porting:
                May need to port the underlying al implementation, depending on
   tools support and specific benchmark use.
*/
int th_sscanf(const char *str, const char *format, ...) {
  int rv;
  va_list args;
  va_start(args, format);
  rv = al_vsscanf(str, format, args);
  va_end(args);
  return rv;
}

/* Function: th_vfscanf
        Create a temporary file

        Porting:
                May need to port the underlying al implementation, depending on
   tools support and specific benchmark use.
*/
int th_vfscanf(ee_FILE *stream, const char *format, va_list ap) {
  return al_vfscanf(stream, format, ap);
}

int th_fscanf(ee_FILE *stream, const char *format, ...) {
  int rv;
  va_list args;
  va_start(args, format);
  rv = al_vfscanf(stream, format, args);
  va_end(args);
  return rv;
}

/* NON Standard routines */

int th_filecmp(const char *file1, const char *file2) {
  return al_filecmp(file1, file2);
}
ee_FILE *th_fcreate(const char *filename, const char *mode, char *data,
                    size_t size) {
  return al_fcreate(filename, mode, data, size);
}
size_t th_fsize(const char *filename) { return al_fsize(filename); }

/**
 * function: th_harness_poll
 *
 * Description:
 *			Stub routine. Can be used for debugging.
 *
 *          This function can be called during the execution of a test
 *          or benchmark to give the test harness some execution time so it
 *          can respond to commands from a host.
 *
 * Note:
 *			Only debug or test versions of true bench marks should
 *call this function as it uses CPU time.
 *
 * Returns:
 *			TRUE if the test should keep running.
 *          FALSE if the stop message has been received form the host
 *          and the benchmark or test should stop.
 */

int th_harness_poll(void) { return (TRUE); }

/**
 * function: th_send_buf_as_file
 *
 * Description:
 *			Sends a buffer (as a file) to the host using ZMODEM
 *
 *          This function sends the contents of a buffer to the host
 *          system using uuencode.
 *
 * Parameters:
 *			buf    - a pointer to the buffer to send
 *          length - the length of the buffer
 *          fn     - the name to use on the host system.
 *
 * RETRUNS:
 *			Success of the file was succesfully sent.
 *          Failure otherwise.
 *
 * Note:
 *			If a file exists on the remote system it will
 *          be overwritten.
 */
n_int th_send_buf_as_file(const unsigned char *buf, BlockSize length,
                          const char *fn) {
  return uu_send_buf(buf, length, fn);
}

/*------------------------------------------------------------------------------
 * function: th_vsprintf
 *
 * Description:
 *			The basic vsprintf function.
 *
 * Parameters:
 *			fmt - the classic vprintf format string
 * ---------------------------------------------------------------------------*/
int th_vsprintf(char *str, const char *fmt, va_list args) {
  return vsprintf(str, fmt, args);
}
/* Function: th_snprintf
        safe version of sprintf.

*/
int th_snprintf(char *str, size_t size, const char *format, ...) {
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsnprintf(str, size, format, args);
  va_end(args);
  return ret;
}

/*------------------------------------------------------------------------------
 * function: th_printf
 *
 * Description:
 *			The basic printf function. Does not need to be used in
 *TH_Lite
 *
 * Parameters:
 *			fmt - the classic printf format string, va's in stdarg.h
 * ---------------------------------------------------------------------------*/

int th_printf(const char *fmt, ...) {
  int rv;
  va_list args;
  va_start(args, fmt);
  rv = al_printf(fmt, args);
  va_end(args);
  return rv;
}

/*------------------------------------------------------------------------------
 * function: th_sprintf
 *
 * Description:
 *			The basic sprintf function.
 *
 * Parameters:
 *			fmt - the classic printf format string
 * ---------------------------------------------------------------------------*/
int th_sprintf(char *str, const char *fmt, ...) {
  int rv;
  va_list args;
  va_start(args, fmt);
  rv = th_vsprintf(str, fmt, args);
  va_end(args);
  return rv;
}

/*==============================================================================
 *                  -- Funcational Layer Implemenation --
 * ===========================================================================*/

/*------------------------------------------------------------------------------
 * Malloc and Free Mapping
 *----------------------------------------------------------------------------*/

#if MAP_MALLOC_TO_TH
/*
 * If malloc or free are called by OS or crt0 th_heap will not be initialized,
 * and we need to give up. Attempt to return Error 8. This traps the case when
 * duplicate heap managers are being compiled into the benchmark code. To
 * minimize size, use compiler malloc in thcfg.h: HAVE_MALLOC=TRUE,
 * COMPILE_OUT_HEAP=TRUE
 */
#if !HAVE_MALLOC && !COMPILE_OUT_HEAP
extern void *th_heap;
#else
/* heap is compiled out */
#error "MAP_MALLOC_TO_TH only valid if using internal heap manager"
#endif

/** malloc entry point to catch CRT0 calls when internal heap manager is used.
 */
void *malloc(size_t foo) {
  if (th_heap)
    return th_malloc(foo);
  else
    exit(THE_OUT_OF_MEMORY);
}

void free(void *ptr) {
  if (th_heap)
    th_free(ptr);
}
#endif

/**
 * function: th_malloc_x
 *
 * Description:
 *			Test Harness malloc()
 *          This works just like malloc() from the standard library.
 *
 * Parameters:
 *			size - is the size of the memory block neded
 *           file - the __FILE__ macro from where the call was made
 *           line - the __LINE__ macro from where the call was made
 *
 * Note:
 *			This function is usually invoked by using th th_malloc()
 *          macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 * Returns:
 *			A void pointer to the allocated block.  Use a cast to
 *asssign it to the proper poitner type. The NULL value is returned if a block
 *ofthe specified size cannot be allocated.
 */

/* coverity[+alloc] */
void *th_malloc_x(size_t size, const char *file, int line)

{
  void *p;
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
  p = malloc(size); /* file, line );*/
#if REPORT_THMALLOC_STATS
  if (size == 0) {
    th_printf("Malloc of 0 from %s:%d\n", file, line);
  }
#endif
#if THDEBUG > 2
  if (p == NULL)
    th_printf("malloc failed from %s:%d\n", file, line);
  th_printf("malloc of %fK %08x from %s:%d\n", (float)size / 1024.0, p, file,
            line);
#endif
#if TH_SAFE_MALLOC
  if (p == NULL)
    th_exit(THE_OUT_OF_MEMORY, "Malloc Failed from %s:%d!\n", file, line);
#endif
#if REPORT_THMALLOC_STATS
  if (size == 0) {
    th_printf("Malloc of 0 from %s:%d\n", file, line);
  }
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  th_malloc_total += size;
  if (th_malloc_max < th_malloc_total)
    th_malloc_max = th_malloc_total;
  ee_list_add(&th_malloc_sizes, p, (unsigned int)size);
  al_mutex_unlock(&th_malloc_mutex);
#endif
  return p;
}

/**
 * function: th_aligned_malloc_x
 *
 * Description:
 *			Test Harness malloc() with pointer alignment, works like
 *vc's _aligned_malloc
 *
 *
 * Parameters:
 *			size - is the size of the memory block neded
 *          align - required alignment for returned pointer
 *           file - the __FILE__ macro from where the call was made
 *           line - the __LINE__ macro from where the call was made
 *
 * Note:
 *			This function is usually invoked by using th th_malloc()
 *          macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 * Returns:
 *			A void pointer to the allocated block.  Use a cast to
 *asssign it to the proper poitner type. The NULL value is returned if a block
 *ofthe specified size cannot be allocated.
 */

/* coverity[+alloc] */
void *th_aligned_malloc_x(size_t size, size_t align, const char *file, int line)

{
  void *p;
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
  p = malloc_aligned(size, align); /* file, line );*/
#if REPORT_THMALLOC_STATS
  if (size == 0) {
    th_printf("Malloc of 0 from %s:%d\n", file, line);
  }
#endif
#if THDEBUG > 2
  if (p == NULL)
    th_printf("malloc failed from %s:%d\n", file, line);
  th_printf("malloc of %fK %08x from %s:%d\n", (float)size / 1024.0, p, file,
            line);
#endif
#if TH_SAFE_MALLOC
  if (p == NULL)
    th_exit(THE_OUT_OF_MEMORY, "Malloc Failed from %s:%d!\n", file, line);
#endif
#if REPORT_THMALLOC_STATS
  if (size == 0) {
    th_printf("Malloc of 0 from %s:%d\n", file, line);
  }
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  th_malloc_total += size;
  if (th_malloc_max < th_malloc_total)
    th_malloc_max = th_malloc_total;
  ee_list_add(&th_malloc_sizes, p, (unsigned int)size);
  al_mutex_unlock(&th_malloc_mutex);
#endif
  return p;
}

/**
 * Function:  th_calloc_x
 *
 * Description:
 *			Test Harness calloc()
 *
 *           This works just like calloc() from the standard library.
 *
 * Parameters:
 *			size - is the size of the memory block neded
 *           file - the __FILE__ macro from where the call was made
 *           line - the __LINE__ macro from where the call was made
 *
 * Note:
 *			This function is usually invoked by using th_calloc()
 *           macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 * Returns:
 *			A void pointer to the allocated block.  Use a cast to
 *asssign it to the proper poitner type. The NULL value is returned if a block
 *ofthe specified size cannot be allocated.
 */

/* coverity[+alloc] */
void *th_calloc_x(size_t nmemb, size_t size, const char *file, int line) {
  void *p;
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
  p = calloc(nmemb, size); /*, file, line );*/
#if THDEBUG > 2
  if (p == NULL)
    th_printf("calloc failed from %s:%d\n", file, line);
  th_printf("calloc of %fK %08x from %s:%d\n", (float)size / 1024.0, p, file,
            line);
#endif
#if REPORT_THMALLOC_STATS
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  th_malloc_total += size;
  if (th_malloc_max < th_malloc_total)
    th_malloc_max = th_malloc_total;
  ee_list_add(&th_malloc_sizes, p, (unsigned int)size);
  al_mutex_unlock(&th_malloc_mutex);
#endif
  return p;
}

/** Function: Test Harness realloc()
 *
 * This works just like realloc() from the standard library.
 *
 * Params:
 *	ptr  - pointer to the original malloc
 *	size - is the size of the memory block neded
 *	file - the __FILE__ macro from where the call was made
 *	line - the __LINE__ macro from where the call was made
 *
 *	Note:
 *	This function is usually invoked by using th th_realloc() macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 *	Returns:
 *		  A void pointer to the allocated block.  Use a cast to asssign
 *        it to the proper poitner type.
 *        The NULL value is returned if a block ofthe specified size cannot
 *        be allocated.
 */
void *th_realloc_x(void *ptr, size_t size, const char *file, int line) {
  void *ret;
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
  ret = realloc(ptr, size);
#if REPORT_THMALLOC_STATS
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  if (ee_list_dsearch(&th_malloc_sizes, ptr)) {
    th_malloc_total -= (*(ee_list_dsearch(&th_malloc_sizes, ptr)))->info;
    th_malloc_total += size;
    if (th_malloc_max < th_malloc_total)
      th_malloc_max = th_malloc_total;
    (*(ee_list_dsearch(&th_malloc_sizes, ptr)))->info = size;
    (*(ee_list_dsearch(&th_malloc_sizes, ptr)))->data = ret;
  }
  al_mutex_unlock(&th_malloc_mutex);
#endif
  return ret;
}
#if (HAVE_STRDUP == 0)
char *strdup(const char *s1) {
  char *str = th_malloc(th_strlen(s1) + 1);
  th_strcpy(str, s1);
  return str;
}
#endif

/** Function: Test Harness strdup()
 *
 * This works just like strdup() from the standard library.
 * Params:
 *	string  - pointer to the original character string
 *	file    - the __FILE__ macro from where the call was made
 *	line    - the __LINE__ macro from where the call was made
 *
 *	Note:
 *	This function is usually invoked by using th th_strdup() macro.
 *
 *	Returns:
 *	A char pointer to the allocated block.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 *	The NULL value is returned if a block of the strlen size cannot
 *	be allocated.
 */
char *th_strdup_x(const char *string, const char *file, int line) {
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
  return strdup(string); /*, file, line )*/
}
/** Function: Test Harness free()
 *
 *           This works just like free() from the standard library.
 *
 * Parameters:
 *			ptr - points to the block pointer returned by
 *th_malloc(). file - the __FILE__ macro from where the call was made line - the
 *__LINE__ macro from where the call was made
 *
 * Note:
 *			This function is usually invoked by using th th_free()
 *           macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 * Note:
 *			It is valid to pass the null pointer to this function.
 */

/* coverity[+free : arg-0] */
void th_free_x(void *block, const char *file, int line) {
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
#if THDEBUG > 2
  th_printf("Free %08x from %s:%d\n", block, file, line);
#endif
#if REPORT_THMALLOC_STATS
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  if (ee_list_dsearch(&th_malloc_sizes, block)) {
    th_malloc_total -= (*(ee_list_dsearch(&th_malloc_sizes, block)))->info;
    ee_list_remove(ee_list_dsearch(&th_malloc_sizes, block));
  }
  al_mutex_unlock(&th_malloc_mutex);
#endif
  free(block); /*, file, line );*/
}

/** Function: Test Harness free() for aligned allocations
 *
 *           This works just like _aligned_free from vc's library
 *
 * Parameters:
 *			ptr - points to the block pointer returned by
 *th_malloc(). file - the __FILE__ macro from where the call was made line - the
 *__LINE__ macro from where the call was made
 *
 * Note:
 *			This function is usually invoked by using th th_free()
 *           macro.
 *
 * Note:
 *			Initial version of MITH relies on libc/OS to provide
 *thread safe implementation for this function.
 *
 * Note:
 *			It is valid to pass the null pointer to this function.
 */

/* coverity[+free : arg-0] */
void th_aligned_free_x(void *block, const char *file, int line) {
  /* avoid warnings since we are not using these */
  file = file;
  line = line;
#if THDEBUG > 2
  th_printf("Free %08x from %s:%d\n", block, file, line);
#endif
#if REPORT_THMALLOC_STATS
  if (th_malloc_sizes == NULL)
    init_thmalloc_stats();
  al_mutex_lock(&th_malloc_mutex);
  if (ee_list_dsearch(&th_malloc_sizes, block)) {
    th_malloc_total -= (*(ee_list_dsearch(&th_malloc_sizes, block)))->info;
    ee_list_remove(ee_list_dsearch(&th_malloc_sizes, block));
  }
  al_mutex_unlock(&th_malloc_mutex);
#endif
  free_aligned(block); /*, file, line );*/
}

/**
 * function: th_heap_reset()
 *
 * Description:
 *			Resets the heap to its initialized state
 *
 *          Calling this function implicitly free's all the memory which
 *          has been allocated with th_malloc()
 */
void th_heap_reset(void) { /* invalid unless using own memory manager*/ }

/* Function: th_strcpy
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strcpy(char *dest, const char *src) { return strcpy(dest, src); }
/* Function: th_strncpy
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strncpy(char *dest, const char *src, size_t n) {
  return strncpy(dest, src, n);
}
/* Functi
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strchr(const char *s, int c) { return strchr(s, c); }
/* Function: th_strrchr
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strrchr(const char *s, int c) { return strrchr(s, c); }
/* Function: th_strstr
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strstr(const char *haystack, const char *needle) {
  return strstr(haystack, needle);
}
/* Function: th_strcmp
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
int th_strcmp(const char *s1, const char *s2) { return strcmp(s1, s2); }
/* Function: th_strncmp
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
int th_strncmp(const char *s1, const char *s2, size_t n) {
  return strncmp(s1, s2, n);
}
/* Function: th_strpbrk
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strpbrk(const char *s, const char *accept) {
  return strpbrk(s, accept);
}
/* Function: th_strcspn
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
size_t th_strcspn(const char *s, const char *reject) {
  return strcspn(s, reject);
}
/* Function: th_strcat
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
char *th_strcat(char *dest, const char *src) { return strcat(dest, src); }
/* Function: th_memcpy
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
void *th_memcpy(void *s1, const void *s2, size_t n) {
#if USE_RVV
  return vec_memcpy(s1, s2, n);
#else
  return memcpy(s1, s2, n);
#endif
}
/* Function: th_memmove
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
void *th_memmove(void *s1, const void *s2, size_t n) {
#if USE_RVV
  return vec_memmove(s1, s2, n);
#else
  return memmove(s1, s2, n);
#endif
}
/* Function: th_memset
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
void *th_memset(void *s, int c, size_t n) {
#if USE_INTERNAL_MEMSET
  char *d = (char *)dest, c = i & 0x0ff;
  while (n > 0)
    *d++ = c;
  return dest;
#else
#if USE_RVV
  return vec_memset(s, c, n);
#else
  return memset(s, c, n);
#endif
#endif
}

/* Function: th_memcmp
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
int th_memcmp(const void *s1, const void *s2, size_t n) {
  return memcmp(s1, s2, n);
}

/* Function: th_strlen
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
size_t th_strlen(const char *s) { return strlen(s); }
/* Function: th_strspn
        Description:
        Initial version of MITH relies on libc/OS to provide thread safe
        implementation for this function.
*/
size_t th_strspn(const char *s, const char *s2) { return strspn(s, s2); }
/* Function: th_isdigit
        Description:
        Return 1 if parameter c is a digit between 0..9, 0 otherwise.
*/
int th_isdigit(int c) { return (((c) >= '0') & ((c) <= '9')); }
/* Function: th_isspace
        Description:
        Return 1 if parameter c is the whitespace char.
*/
int th_isspace(int c) { return ((c) == ' '); }
/* Function: th_strtol
        string to int.

        Redirect to stdlib in initial version of mith.
*/
long int th_strtol(const char *nptr, char **endptr, int base) {
  return strtol(nptr, endptr, base);
}
/* Function: th_atoi
        string to int.

        Redirect to stdlib in initial version of mith.
*/
int th_atoi(const char *nptr) { return atoi(nptr); }
/* Function: th_abs
        absolute value.

        Redirect to stdlib in initial version of mith.
*/
int th_abs(int j) { return abs(j); }
#if FLOAT_SUPPORT
/* Function: th_strtol
        string to double.

        Redirect to stdlib in initial version of mith.
*/
double th_strtod(const char *nptr, char **RESTRICT endptr) {
  return strtod(nptr, endptr);
}
/* Function: th_atof
        string to int.

        Redirect to stdlib in initial version of mith.
*/
double th_atof(const char *nptr) { return strtod(nptr, NULL); }
#endif

e_s32 th_parse_val_type(char *valstring, e_s32 *sval, e_u32 *uval) {
  e_u32 retval = 0;
  e_s32 retval_signed;
  e_s32 neg = 1;
  e_s32 hexmode = 0;
  if (*valstring == '-') {
    neg = -1;
    valstring++;
  }
  if ((valstring[0] == '0') && (valstring[1] == 'x')) {
    hexmode = 1;
    valstring += 2;
  }
  /* first look for digits */
  if (hexmode) {
    while (((*valstring >= '0') && (*valstring <= '9')) ||
           ((*valstring >= 'a') && (*valstring <= 'f'))) {
      e_s32 digit = *valstring - '0';
      if (digit > 9)
        digit = 10 + *valstring - 'a';
      retval *= 16;
      retval += digit;
      valstring++;
    }
  } else {
    while ((*valstring >= '0') && (*valstring <= '9')) {
      e_s32 digit = *valstring - '0';
      retval *= 10;
      retval += digit;
      valstring++;
    }
  }
#define C_1K ((e_u32)1024)
#define C_1kd ((e_u32)1000)
  /* now add qualifiers */
  if (*valstring == 'K')
    retval *= C_1K;
  if (*valstring == 'M')
    retval *= C_1K * C_1K;
  if (*valstring == 'G')
    retval *= C_1K * C_1K * C_1K;
  if (*valstring == 'k')
    retval *= C_1kd;
  if (*valstring == 'm')
    retval *= C_1kd * C_1kd;
  if (*valstring == 'g')
    retval *= C_1kd * C_1kd * C_1kd;

  retval_signed = ((e_s32)retval) * neg;
  if (sval != NULL)
    *sval = retval_signed;
  if (uval != NULL)
    *uval = retval;
  if (uval != NULL)
    return retval;
  else
    return retval_signed;
}
/* Function: th_parse_val
        th function to parse an int argument, with optional Meg/Kilo qualifiers

Params:
        valstring - string to parse, must be non-NULL

Returns:
        value parsed
*/
e_s32 th_parse_val(char *valstring) {
  e_s32 ret = 0;
  th_parse_val_type(valstring, &ret, NULL);
  return ret;
}
/* Function: th_parse_val_unsigned
        th function to parse an unsigned int argument, with optional Meg/Kilo
qualifiers

Params:
        valstring - string to parse, must be non-NULL

Returns:
        value parsed
*/
e_u32 th_parse_val_unsigned(char *valstring) {
  e_u32 ret = 0;
  th_parse_val_type(valstring, NULL, &ret);
  return ret;
}

/* Function: th_parse_flag
        Helper function to parse argvals for a flag value

Params:
        argc/argv - command line args
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf
*/
int th_parse_flag(int argc, char *argv[], char *flag, e_s32 *val) {
  int i;
  for (i = 0; i < argc; i++) {
    char *arg = argv[i];
    if (th_strncmp(arg, flag, strlen(flag)) == 0) {
      *val = th_parse_val(arg + strlen(flag));
      return 1;
    }
  }
  return 0;
}

/* Function: th_parse_flag_unsigned
        Helper function to parse argvals for a flag value

Params:
        argc/argv - command line args
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf
*/
int th_parse_flag_unsigned(int argc, char *argv[], char *flag, e_u32 *val) {
  int i;
  for (i = 0; i < argc; i++) {
    char *arg = argv[i];
    if (th_strncmp(arg, flag, strlen(flag)) == 0) {
      *val = th_parse_val_unsigned(arg + strlen(flag));
      return 1;
    }
  }
  return 0;
}

/* Function: th_get_flag
        Helper function to search argvals for a flag

Params:
        argc/argv - command line args
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf
*/
int th_get_flag(int argc, char *argv[], char *flag, char **val) {
  int i;
  for (i = 0; i < argc; i++) {
    char *arg = argv[i];
    if (th_strncmp(arg, flag, strlen(flag)) == 0) {
      *val = arg + th_strlen(flag);
      return 1;
    }
  }
  return 0;
}
/* Function: th_parse_buf_flag
        Helper function to search a string for a flag value

Params:
        buf - buffer to search
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf
*/
int th_parse_buf_flag(char *buf, char *flag, e_s32 *val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    *val = th_parse_val(p + strlen(flag));
    return 1;
  }
  return 0;
}

int th_parse_buf_flag_unsigned(char *buf, char *flag, e_u32 *val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    *val = th_parse_val_unsigned(p + strlen(flag));
    return 1;
  }
  return 0;
}

int th_parse_buf_f64flag(char *buf, char *flag, e_f64 *val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    th_sscanf(p + strlen(flag), "%f", val);
    return 1;
  }
  return 0;
}

int th_parse_buf_f32flag(char *buf, char *flag, e_f32 *val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    th_sscanf(p + strlen(flag), "%f", val);
    return 1;
  }
  return 0;
}
/* Function: th_get_buf_flag
        Helper function to search a string for a flag

Params:
        buf - buffer to search
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf, position in string after the
flag.
*/
int th_get_buf_flag(char *buf, char *flag, char **val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    *val = p + th_strlen(flag);
    return 1;
  }
  return 0;
}

/* Function: th_get_buf_flag
        Helper function to search a string for a flag

Params:
        buf - buffer to search
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf, position in string after the
flag.
*/
int th_dup_buf_flag(char *buf, char *flag, char **val) {
  char *p;
  if (buf == NULL)
    return 0;
  p = th_strstr(buf, flag);
  if (p) {
    char tmpbuf[64];
    char *endflag = th_strstr(p, " ");
    size_t flaglen = th_strlen(flag);
    size_t length;
    if (endflag == NULL)
      length = th_strlen(p) - flaglen;
    else
      length = (e_u32)(endflag - p) - flaglen;
    strncpy(tmpbuf, p + flaglen, length);
    tmpbuf[length] = 0;
    *val = th_strdup(tmpbuf);
    return 1;
  }
  return 0;
}

/* Function: th_parse_buf_flag_word
        Helper function to search a string for a flag value which is a single %s

Params:
        buf - buffer to search
        flag - flag to search in buffer, must be non-NULL
        val - return value if flag is found in buf, position in string after the
flag.
        * note, val must be preallocated!
*/
int th_parse_buf_flag_word(char *buf, char *flag, char *val[]) {
  char *pval;
  int res = th_get_buf_flag(buf, flag, &pval);
  if (res) {
    th_sscanf(pval, "%s", val);
  }
  return res;
}

/********************************************************************
Function:  calc_crc
Purpose:
        Compute crc16 a byte at a time
********************************************************************/

#if CRC_CHECK || NON_INTRUSIVE_CRC_CHECK

e_u16 Calc_crc8(e_u8 data, e_u16 crc) {
  e_u8 i = 0, x16 = 0, carry = 0;

  for (i = 0; i < 8; i++) {
    x16 = (e_u8)((data & 1) ^ ((e_u8)crc & 1));
    data >>= 1;

    if (x16 == 1) {
      crc ^= 0x4002;
      carry = 1;
    } else
      carry = 0;
    crc >>= 1;
    if (carry)
      crc |= 0x8000;
    else
      crc &= 0x7fff;
  }
  return crc;
}
/*********************************************************************/
e_u16 Calc_crc16(e_u16 data, e_u16 crc) {

  crc = Calc_crc8((e_u8)(data & 0x00FF), crc);
  crc = Calc_crc8((e_u8)((data & 0xFF00) >> 8), crc);
  return crc;
}

/*********************************************************************/
e_u16 Calc_crc32(e_u32 data, e_u16 crc) {

  crc = Calc_crc8((e_u8)(data & 0x000000FF), crc);
  crc = Calc_crc8((e_u8)((data & 0x0000FF00) >> 8), crc);
  crc = Calc_crc8((e_u8)((data & 0x00FF0000) >> 16), crc);
  crc = Calc_crc8((e_u8)((data & 0xFF000000) >> 24), crc);

  return crc;
}
/* Function: th_crcbuffer
        Compute crc16 of a buffer.

A CRC is calculated of an input buffer.
Multiple buffer CRC checking is also supported for streaming
output data.

For a single buffer the input CRC MUST be 0, and the final
CRC result is returned.

In the muliple buffer case, the input CRC is used as an intermediate
value for calculating the final CRC result.
The first buffer is passed with a inputCRC
of 0 as in the single buffer case. The subsequent buffers are
passed with the CRC result of the previous buffer calculation.

The caller must save the CRC result value and pass it
back in inputCRC for the additional buffers.

The original usage is that this routine is called
after a th_fsize and th_fread of a file into a buffer. The size and
buffer parameters are compatible with th_fsize and th_fread types.

Note: Addressability is not assumed, but the data is assumed
to be unpacked. Packing can be
handled by using crc16, or crc32 routines. A Local fix is allowed.
However, it is likely that other dependencies have already caused
the buffer to be unpacked. When porting, You must ensure that the buffer
is packed before applying a local fix. When calculating
CRC's on packed data, you may also
have endian issues.

Params:
- inbuf
        Input buffer, usually e_u8* of data to CRC check.
- size
        Number of e_u8's to CRC.
- inputCRC
        Initial CRC value for inbuf. 0 for the first buffer.

        For multiple buffers, the CRC of the previous call must be used. The
        caller is responsible for saving this value.
- retval
        CRC of inbuf based on inputCRC.
*/
e_u16 th_crcbuffer(const void *inbuf, size_t size, e_u16 inputCRC) {
  e_u16 CRC = inputCRC;
  e_u8 *buf = (void *)inbuf;
  size_t i;

  /*
   * Allow the AND case if found to be necessary.
   * Allow AND case being a NULL inbuf, and 0 size buffer where the inputCRC is
   * returned.
   */
  if (!buf && !size)
    return 0;
  else if (!buf || !size) {
    th_printf("\nFailure: Attempt to CRC an empty buffer, inbuf:%x size:%d.\n",
              (size_t)buf, size);
    return 0;
  }

  for (i = 0; i < size; i++) {
    CRC = Calc_crc8(*buf, CRC);
    buf++;
  }
  return CRC;
}
#endif

#if (FP_KERNELS_SUPPORT)
e_f64 ee_snr_buffer_spdp(e_f64 *signal, e_f32 *signal_sp, e_f64 *ref,
                         e_f32 *ref_sp, int size, snr_result *res) {
  int i;
  e_f64 snr = 0.0;
  res->max = 0.0;
  res->sum = 0.0;
  res->min = 1.0e+20;
  for (i = 0; i < size; i++) {
    if (signal != NULL)
      snr = (e_f64)ee_snr_db(signal[i], ref[i]);
    else
      snr = (e_f64)ee_snr_db(signal_sp[i], ref_sp[i]);
    if (snr > res->max)
      res->max = snr;
    if (snr < res->min)
      res->min = snr;
    res->sum += snr;
  }
  res->avg = res->sum / size;
  if (res->min < res->min_ok) {
    res->pass = 0;
  } else {
    res->pass = 1;
  }
  return res->min;
}
intparts intparts_zero = {0, 1, 0, 0};
e_u32 ee_accurate_bits_helper(e_f64 *signal, e_f32 *signal_sp, e_f64 *ref,
                              e_f32 *ref_sp, int size, snr_result *res) {
  int i;
  e_u32 bits = 0;
  res->bmax = 0;
  res->bsum = 0;
  res->bmin = 1000;
  for (i = 0; i < size; i++) {
    if (signal != NULL)
      bits = fp_accurate_bits_dp(signal[i], ref[i]);
    else
      bits = fp_accurate_bits_sp(signal_sp[i], ref_sp[i]);
    if (bits > res->bmax)
      res->bmax = bits;
    if (bits < res->bmin)
      res->bmin = bits;
    res->bsum += bits;
  }
  res->bavg = res->bsum / size;
  if (res->bmin < res->bmin_ok) {
    res->pass = 0;
  } else {
    res->pass = 1;
  }
  return res->bmin;
}
e_u32 ee_iaccurate_bits_helper(e_f64 *signal, e_f32 *signal_sp, intparts *ref,
                               int size, snr_result *res) {
  int i;
  e_u32 bits = 0;
  res->bmax = 0;
  res->bsum = 0;
  res->bmin = 1000;
  for (i = 0; i < size; i++) {
    if (signal != NULL)
      bits = fp_iaccurate_bits_dp(signal[i], &ref[i]);
    else
      bits = fp_iaccurate_bits_sp(signal_sp[i], &ref[i]);
    if (bits > res->bmax)
      res->bmax = bits;
    if (bits < res->bmin)
      res->bmin = bits;
    res->bsum += bits;
  }
  res->bavg = res->bsum / size;
  if (res->bmin < res->bmin_ok) {
    res->pass = 0;
  } else {
    res->pass = 1;
  }
  return res->bmin;
}
e_u32 ee_fpbits_buffer_dp(e_f64 *signal, e_f64 *ref, int size,
                          snr_result *res) {
  return ee_accurate_bits_helper(signal, NULL, ref, NULL, size, res);
}
e_u32 ee_fpbits_buffer_sp(e_f32 *signal, e_f32 *ref, int size,
                          snr_result *res) {
  return ee_accurate_bits_helper(NULL, signal, NULL, ref, size, res);
}
e_u32 ee_ifpbits_buffer_dp(e_f64 *signal, intparts *ref, int size,
                           snr_result *res) {
  return ee_iaccurate_bits_helper(signal, NULL, ref, size, res);
}
e_u32 ee_ifpbits_buffer_sp(e_f32 *signal, intparts *ref, int size,
                           snr_result *res) {
  return ee_iaccurate_bits_helper(NULL, signal, ref, size, res);
}
e_f64 ee_snr_buffer(e_f64 *signal, e_f64 *ref, int size, snr_result *res) {
  return ee_snr_buffer_spdp(signal, NULL, ref, NULL, size, res);
}
e_f32 ee_snr_buffer_sp(e_f32 *signal, e_f32 *ref, int size, snr_result *res) {
  return (e_f32)ee_snr_buffer_spdp(NULL, signal, NULL, ref, size, res);
}
int th_print_dp(e_f64 value) {
  intparts num;
  int st = load_dp(&value, &num);
  if (!st)
    return 0;
  th_printf("{%u,%d,0x%08x,0x%08x}/*%1.18e*/", num.sign, num.exp,
            num.mant_high32, num.mant_low32, value);
  return 1;
}
char *th_sprint_dp(e_f64 value, char *buf) {
  intparts num;
  int st = load_dp(&value, &num);
  if (!st)
    return 0;
  th_sprintf(buf, "{%u,%d,0x%08x,0x%08x}/*%1.18e*/", num.sign, num.exp,
             num.mant_high32, num.mant_low32, value);
  return buf;
}
int th_print_sp(e_f32 value) {
  intparts num;
  int st = load_sp(&value, &num);
  if (!st)
    return 0;
  th_printf("{%u,%d,0x%08x,0x%08x}/*%1.18e*/", num.sign, num.exp,
            num.mant_high32, num.mant_low32, value);
  return 1;
}
char *th_sprint_sp(e_f32 value, char *buf) {
  intparts num;
  int st = load_sp(&value, &num);
  if (!st)
    return 0;
  th_sprintf(buf, "{%u,%d,0x%08x,0x%08x}/*%1.18e*/", num.sign, num.exp,
             num.mant_high32, num.mant_low32, value);
  return buf;
}

int th_fpprintf(const char *fmt, ...) {
  va_list args;
  int rv, l = th_strlen(fmt);
  char cfmt[512];
  if (l < 500) {
    const char *p = fmt;
    char *po = cfmt;
    while (*p) {
      *po = *p;
      if ((p[0] == '%') && (p[1] == 'z') && (p[2] == 'e')) {
#if USE_FP32
        memcpy(po, "%1.20", 5);
        po[5] = 'e';
        po[6] = 'f';
#endif
#if USE_FP64
        memcpy(po, "%1.40", 5);
        po[5] = 'l';
        po[6] = 'e';
#endif
        p += 2;
        po += 6;
      }
      p++;
      po++;
    }
    *po = 0;
    va_start(args, fmt);
    rv = al_printf(cfmt, args);
    va_end(args);
  } else {
    va_start(args, fmt);
    rv = al_printf(fmt, args);
    va_end(args);
  }
  return rv;
}
#endif

char *th_err_code[] = {"Invalid", "Info", "Warning", "ERROR", "FATAL"};
/* Function: th_log
        Description:
        Log a message with test harness qualifiers

        Note:
                code must be one of the codes in th_err_codes
*/
void th_log(int code, char *msg) {
  if (code < reporting_threshold || code > TH_ERR_CODE_LAST)
    return;
  th_printf("-  %s: %s\n", th_err_code[code], msg);
}

const int th_rand_data[] = {
    0x32, 0x6b, 0x2d, 0x39, 0x28, 0x71, 0x01, 0x4a, 0x14, 0x31, 0x58, 0x07,
    0x73, 0x14, 0x14, 0x44, 0x4d, 0x4a, 0x22, 0x31, 0x25, 0x5f, 0x26, 0x09,
    0x33, 0x6d, 0x78, 0x54, 0x6c, 0x00, 0x3b, 0x44, 0x64, 0x21, 0x7d, 0x27,
    0x4c, 0x4d, 0x1b, 0x71, 0x26, 0x13, 0x2b, 0x31, 0x52, 0x60, 0x4d, 0x44,
    0x3a, 0x53, 0x29, 0x79, 0x2f, 0x78, 0x00, 0x42, 0x22, 0x03, 0x4b, 0x1a,
    0x70, 0x07, 0x21, 0x26, 0x72, 0x3f, 0x5a, 0x24, 0x6e, 0x56, 0x3a, 0x7a,
    0x63, 0x30, 0x1d, 0x2d, 0x26, 0x55, 0x5c, 0x48, 0x69, 0x31, 0x68, 0x6c,
    0x17, 0x78, 0x36, 0x42, 0x08, 0x74, 0x70, 0x61, 0x33, 0x58, 0x61, 0x33,
    0x10, 0x3e, 0x1c, 0x6f, 0x43, 0x00, 0x6e, 0x02, 0x68, 0x1f, 0x28, 0x7b,
    0x77, 0x67, 0x3e, 0x1c, 0x49, 0x24, 0x29, 0x21, 0x16, 0x00, 0x05, 0x1e,
    0x35, 0x59, 0x1c, 0x40, 0x08, 0x32, 0x3d, 0x1b, 0x1c, 0x75, 0x2c, 0x18,
    0x1b, 0x51, 0x06, 0x64, 0x03, 0x38, 0x16, 0x77, 0x74, 0x3c, 0x6f, 0x59,
    0x77, 0x3a, 0x33, 0x72, 0x58, 0x6b, 0x5e, 0x53, 0x56, 0x49, 0x22, 0x77,
    0x54, 0x06, 0x2f, 0x4f, 0x13, 0x30, 0x52, 0x03, 0x6b, 0x09, 0x5f, 0x20,
    0x73, 0x30, 0x28, 0x1b, 0x53, 0x20, 0x1d, 0x20, 0x78, 0x11, 0x22, 0x46,
    0x29, 0x6e, 0x25, 0x57, 0x6a, 0x70, 0x53, 0x09, 0x73, 0x20, 0x4e, 0x6b,
    0x6a, 0x2f, 0x60, 0x0d, 0x6c, 0x47, 0x6d, 0x2b, 0x58, 0x2c, 0x72, 0x7a,
    0x0f, 0x7d, 0x07, 0x4e, 0x04, 0x30, 0x43, 0x24, 0x47, 0x4d, 0x68, 0x39,
    0x03, 0x3c, 0x24, 0x25, 0x19, 0x02, 0x6a, 0x49, 0x0d, 0x5d, 0x0f, 0x1c,
    0x79, 0x5e, 0x69, 0x69, 0x20, 0x20, 0x2b, 0x31, 0x43, 0x22, 0x33, 0x6f,
    0x05, 0x25, 0x32, 0x47, 0x27, 0x69, 0x3c, 0x0b, 0x21, 0x75, 0x7d, 0x2a,
    0x73, 0x1e, 0x2f, 0x60, 0x3a, 0x73, 0x36, 0x48, 0x7c, 0x2d, 0x37, 0x16,
    0x1b, 0x2b, 0x3a, 0x05, 0x57, 0x07, 0x46, 0x28, 0x22, 0x40, 0x08, 0x7b,
    0x5d, 0x67, 0x13, 0x5a, 0x5d, 0x6b, 0x4f, 0x7a, 0x3d, 0x43, 0x03, 0x4d,
    0x06, 0x4b, 0x33, 0x73, 0x2a, 0x27, 0x6d, 0x5d, 0x0c, 0x2b, 0x33, 0x7e,
    0x50, 0x6a, 0x37, 0x65, 0x07, 0x11, 0x45, 0x77, 0x71, 0x31, 0x35, 0x6d,
    0x24, 0x0d, 0x49, 0x63, 0x12, 0x67, 0x5f, 0x34, 0x3c, 0x2c, 0x35, 0x03,
    0x43, 0x5b, 0x1e, 0x58, 0x63, 0x61, 0x0f, 0x2f, 0x76, 0x62, 0x25, 0x7d,
    0x36, 0x04, 0x4c, 0x01, 0x60, 0x10, 0x2f, 0x1b, 0x49, 0x1a, 0x64, 0x5f,
    0x3a, 0x24, 0x6c, 0x39, 0x6a, 0x37, 0x2a, 0x79, 0x1f, 0x2d, 0x35, 0x3f,
    0x71, 0x48, 0x57, 0x7b, 0x17, 0x48, 0x03, 0x1a, 0x14, 0x46, 0x1a, 0x1b,
    0x08, 0x18, 0x73, 0x64, 0x26, 0x36, 0x7e, 0x66, 0x31, 0x17, 0x1c, 0x5e,
    0x44, 0x55, 0x19, 0x3c, 0x48, 0x0c, 0x23, 0x02, 0x48, 0x71, 0x08, 0x07,
    0x20, 0x27, 0x72, 0x11, 0x79, 0x36, 0x0a, 0x64, 0x38, 0x35, 0x59, 0x18,
    0x10, 0x1f, 0x54, 0x11, 0x29, 0x69, 0x01, 0x76, 0x30, 0x4a, 0x21, 0x34,
    0x58, 0x60, 0x15, 0x10, 0x14, 0x25, 0x6d, 0x23, 0x58, 0x6b, 0x68, 0x06,
    0x09, 0x37, 0x5f, 0x13, 0x34, 0x0d, 0x61, 0x52, 0x5e, 0x46, 0x0c, 0x5d,
    0x43, 0x42, 0x24, 0x79, 0x36, 0x3f, 0x59, 0x21, 0x67, 0x70, 0x00, 0x0d,
    0x22, 0x7d, 0x2e, 0x68, 0x6d, 0x25, 0x7f, 0x01, 0x08, 0x7b, 0x57, 0x26,
    0x5c, 0x76, 0x53, 0x75, 0x44, 0x7d, 0x1d, 0x35, 0x4a, 0x63, 0x19, 0x73,
    0x10, 0x76, 0x61, 0x3d, 0x47, 0x01, 0x31, 0x30, 0x5d, 0x45, 0x56, 0x4b,
    0x24, 0x0d, 0x26, 0x43, 0x73, 0x37, 0x79, 0x1d, 0x42, 0x4e, 0x18, 0x7d,
    0x40, 0x14, 0x45, 0x18, 0x1f, 0x2a, 0x13, 0x4f, 0x1b, 0x78, 0x4f, 0x1b,
    0x3c, 0x6b, 0x38, 0x0f, 0x75, 0x56, 0x15, 0x63, 0x5a, 0x17, 0x63, 0x1b,
    0x20, 0x54, 0x7a, 0x78, 0x67, 0x66, 0x7e, 0x59, 0x55, 0x0f, 0x58, 0x13,
    0x29, 0x39, 0x34, 0x40, 0x6c, 0x69, 0x48, 0x58, 0x11, 0x6c, 0x59, 0x1a,
    0x08, 0x38, 0x6c, 0x26, 0x2b, 0x44, 0x04, 0x70, 0x17, 0x7b, 0x5d, 0x38,
    0x66, 0x3f, 0x2a, 0x13, 0x5e, 0x39, 0x2c, 0x4f, 0x78, 0x11, 0x61, 0x05,
    0x4b, 0x54, 0x01, 0x7b, 0x2b, 0x6c, 0x4e, 0x01, 0x6d, 0x41, 0x1e, 0x55,
    0x66, 0x07, 0x61, 0x4f, 0x76, 0x2e, 0x1a, 0x29, 0x47, 0x6f, 0x79, 0x03,
    0x33, 0x62, 0x54, 0x6f, 0x65, 0x48, 0x4b, 0x49, 0x49, 0x23, 0x5f, 0x35,
    0x66, 0x00, 0x15, 0x44, 0x7b, 0x76, 0x0c, 0x65, 0x29, 0x6b, 0x05, 0x30,
    0x15, 0x33, 0x13, 0x29, 0x3d, 0x52, 0x0f, 0x51, 0x5f, 0x71, 0x72, 0x0c,
    0x70, 0x32, 0x01, 0x63, 0x40, 0x0f, 0x0a, 0x46, 0x09, 0x63, 0x13, 0x34,
    0x03, 0x68, 0x50, 0x14, 0x4e, 0x47, 0x71, 0x3d, 0x2b, 0x0e, 0x0a, 0x3d,
    0x10, 0x55, 0x58, 0x00, 0x23, 0x24, 0x63, 0x7c, 0x12, 0x2c, 0x3b, 0x2c,
    0x72, 0x32, 0x16, 0x49, 0x76, 0x28, 0x3f, 0x1c, 0x37, 0x5b, 0x48, 0x76,
    0x75, 0x32, 0x40, 0x50, 0x36, 0x4e, 0x5a, 0x1f, 0x70, 0x20, 0x2f, 0x48,
    0x1b, 0x22, 0x02, 0x35, 0x51, 0x53, 0x22, 0x3c, 0x07, 0x1a, 0x23, 0x21,
    0x6b, 0x60, 0x1e, 0x5b, 0x46, 0x63, 0x58, 0x3d, 0x6b, 0x0e, 0x66, 0x6e,
    0x66, 0x66, 0x42, 0x1d, 0x70, 0x50, 0x7f, 0x7e, 0x23, 0x3a, 0x3a, 0x7c,
    0x44, 0x63, 0x5a, 0x38, 0x37, 0x02, 0x18, 0x4b, 0x10, 0x05, 0x4b, 0x26,
    0x6d, 0x45, 0x2c, 0x66, 0x2a, 0x49, 0x55, 0x06, 0x4e, 0x5a, 0x66, 0x75,
    0x73, 0x14, 0x09, 0x44, 0x5b, 0x7b, 0x3b, 0x32, 0x5e, 0x7c, 0x09, 0x00,
    0x4b, 0x7f, 0x4b, 0x5b, 0x09, 0x49, 0x57, 0x3a, 0x38, 0x60, 0x22, 0x2b,
    0x31, 0x38, 0x34, 0x20, 0x0e, 0x7d, 0x68, 0x73, 0x3b, 0x55, 0x1e, 0x48,
    0x7a, 0x70, 0x76, 0x70, 0x2e, 0x13, 0x1e, 0x02, 0x4b, 0x5c, 0x49, 0x51,
    0x5a, 0x37, 0x28, 0x3a, 0x33, 0x4a, 0x34, 0x5f, 0x7c, 0x2f, 0x04, 0x55,
    0x49, 0x7d, 0x3d, 0x1b, 0x68, 0x42, 0x03, 0x51, 0x2f, 0x3b, 0x41, 0x37,
    0x50, 0x4a, 0x13, 0x5d, 0x07, 0x69, 0x0f, 0x1e, 0x4b, 0x32, 0x33, 0x75,
    0x54, 0x75, 0x16, 0x01, 0x4e, 0x23, 0x55, 0x3a, 0x1b, 0x03, 0x08, 0x15,
    0x21, 0x71, 0x4a, 0x57, 0x60, 0x4f, 0x42, 0x77, 0x6e, 0x6f, 0x72, 0x26,
    0x47, 0x78, 0x5d, 0x08, 0x27, 0x45, 0x40, 0x63, 0x53, 0x15, 0x4e, 0x47,
    0x1f, 0x1d, 0x65, 0x0e, 0x4f, 0x1c, 0x40, 0x16, 0x23, 0x54, 0x52, 0x65,
    0x6c, 0x10, 0x78, 0x29, 0x64, 0x66, 0x53, 0x0d, 0x58, 0x4c, 0x76, 0x36,
    0x73, 0x58, 0x7e, 0x7f, 0x34, 0x03, 0x78, 0x5a, 0x61, 0x28, 0x54, 0x60,
    0x16, 0x3f, 0x5d, 0x1e, 0x00, 0x4d, 0x3a, 0x0c, 0x4e, 0x0d, 0x1a, 0x41,
    0x6f, 0x3c, 0x21, 0x19, 0x45, 0x36, 0x0c, 0x18, 0x79, 0x29, 0x21, 0x5c,
    0x46, 0x63, 0x10, 0x06, 0x59, 0x15, 0x3e, 0x74, 0x0a, 0x15, 0x44, 0x26,
    0x46, 0x1a, 0x03, 0x20, 0x49, 0x1e, 0x0f, 0x2b, 0x12, 0x1c, 0x50, 0x43,
    0x68, 0x27, 0x05, 0x01, 0x63, 0x3a, 0x54, 0x23, 0x51, 0x49, 0x61, 0x20,
    0x5f, 0x3d, 0x6f, 0x24, 0x28, 0x5d, 0x46, 0x23, 0x52, 0x52, 0x1e, 0x02,
    0x68, 0x23, 0x32, 0x2a, 0x71, 0x76, 0x65, 0x2c, 0x24, 0x72, 0x72, 0x18,
    0x12, 0x75, 0x38, 0x31, 0x3f, 0x1d, 0x7e, 0x60, 0x2d, 0x11, 0x78, 0x7c,
    0x13, 0x1d, 0x57, 0x07, 0x08, 0x42, 0x29, 0x37, 0x17, 0x21, 0x3d, 0x40,
    0x69, 0x0a, 0x65, 0x31, 0x01, 0x75, 0x69, 0x79, 0x0c, 0x50, 0x30, 0x39,
    0x6a, 0x07, 0x5e, 0x72, 0x0e, 0x30, 0x60, 0x56, 0x58, 0x5f, 0x57, 0x05,
    0x41, 0x63, 0x1d, 0x08, 0x06, 0x01, 0x78, 0x71, 0x2a, 0x17, 0x17, 0x1c,
    0x66, 0x01, 0x02, 0x30, 0x30, 0x1f, 0x3a, 0x79, 0x67, 0x11, 0x7d, 0x77,
    0x29, 0x49, 0x37, 0x2f, 0x76, 0x45, 0x1d, 0x5c, 0x14, 0x20, 0x26, 0x77,
    0x22, 0x2e, 0x11, 0x19, 0x7d, 0x7c, 0x3b, 0x31, 0x3c, 0x29, 0x42, 0x60,
    0x74, 0x68, 0x1f, 0x70, 0x02, 0x51, 0x2b, 0x3d, 0x21, 0x02, 0x44, 0x7a,
    0x62, 0x2a, 0x07, 0x65, 0x59, 0x43, 0x6e, 0x35, 0x63, 0x5b, 0x3b, 0x12,
    0x11, 0x79, 0x7e, 0x56, 0x2e, 0x44, 0x22, 0x4f, 0x6c, 0x69, 0x23, 0x35,
    0x29, 0x51, 0x57, 0x6c, 0x56, 0x28, 0x13, 0x2f, 0x59, 0x14, 0x55, 0x2a,
    0x0a, 0x27, 0x14, 0x6f, 0x28, 0x1a, 0x6a, 0x78, 0x7f, 0x5d, 0x70, 0x17,
    0x4f, 0x5e, 0x08, 0x0e, 0x61, 0x1b, 0x14, 0x3e, 0x0c, 0x76, 0x0f, 0x67,
    0x79, 0x69, 0x21, 0x57, 0x2a, 0x37, 0x04, 0x39, 0x59, 0x17, 0x12, 0x1f,
    0x67, 0x24, 0x6c, 0x3a, 0x42, 0x5c, 0x3a, 0x4a, 0x62, 0x5f, 0x60, 0x10,
    0x2c, 0x6b, 0x59, 0x52, 0x2c, 0x2b, 0x71, 0x1f, 0x42, 0x54, 0x69, 0x55,
    0x48, 0x48, 0x76, 0x04, 0x24, 0x75, 0x53, 0x31, 0x75, 0x78, 0x09, 0x3f,
    0x75, 0x4c, 0x4e, 0x0f, 0x00, 0x19, 0x19, 0x5b, 0x21, 0x6a, 0x5a, 0x6b,
    0x27, 0x0a, 0x5d, 0x21, 0x7d, 0x68, 0x0b, 0x2b, 0x5a, 0x61, 0x1f, 0x00,
    0x23, 0x04, 0x32, 0x30, 0x14, 0x1c, 0x08, 0x64, 0x02, 0x0c, 0x58, 0x69,
    0x6a, 0x11, 0x50, 0x2c, 0x40, 0x3b, 0x70, 0x41, 0x64, 0x46, 0x11, 0x0f,
    0x30, 0x60, 0x55, 0x78, 0x15, 0x35, 0x73, 0x4c, 0x73, 0x05, 0x31, 0x3d,
    0x22, 0x41, 0x44, 0x0e, 0x0e, 0x53, 0x2d, 0x34, 0x31, 0x31, 0x5a, 0x2d,
    0x75, 0x74, 0x27, 0x44, 0x05, 0x14, 0x0a, 0x57, 0x1e, 0x12, 0x6c, 0x7b,
    0x13, 0x54, 0x62, 0x17, 0x52, 0x71, 0x15, 0x1d, 0x0e, 0x05, 0x17, 0x0c,
    0x64, 0x28, 0x7b, 0x3e, 0x0e, 0x2d, 0x7e, 0x1f, 0x1e, 0x38, 0x0c, 0x3c,
    0x42, 0x5a, 0x4d, 0x59, 0x65, 0x13, 0x4c, 0x49, 0x69, 0x43, 0x17, 0x15,
    0x45, 0x03, 0x37, 0x04, 0x6c, 0x15, 0x60, 0x15, 0x14, 0x3d, 0x72, 0x31,
    0x10, 0x59, 0x08, 0x21, 0x1c, 0x50, 0x4d, 0x64, 0x71, 0x58, 0x70, 0x40,
    0x73, 0x60, 0x6e, 0x08, 0x4b, 0x1d, 0x3d, 0x2a, 0x66, 0x69, 0x3e, 0x7d,
    0x4d, 0x75, 0x57, 0x0e, 0x13, 0x01, 0x30, 0x26, 0x78, 0x2e, 0x0e, 0x5d,
    0x36, 0x71, 0x63, 0x1f, 0x0f, 0x12, 0x18, 0x59, 0x17, 0x59, 0x4b, 0x5c,
    0x64, 0x0f, 0x0a, 0x2a, 0x23, 0x3c, 0x0f, 0x0f, 0x35, 0x1e, 0x01, 0x77,
    0x0b, 0x08, 0x2b, 0x0d, 0x6b, 0x43, 0x18, 0x5b, 0x79, 0x07, 0x0e, 0x27,
    0x6d, 0x42, 0x27, 0x0e, 0x72, 0x35, 0x7b, 0x5e, 0x40, 0x5a, 0x57, 0x4e,
    0x1b, 0x1d, 0x24, 0x7c, 0x61, 0x4a, 0x78, 0x17, 0x33, 0x28, 0x57, 0x30,
    0x52, 0x70, 0x6d, 0x34, 0x71, 0x2e, 0x60, 0x65, 0x5a, 0x7f, 0x6d, 0x6a,
    0x78, 0x4f, 0x34, 0x44, 0x68, 0x2a, 0x70, 0x34, 0x75, 0x2a, 0x15, 0x45,
    0x1a, 0x13, 0x42, 0x52, 0x5a, 0x7b, 0x50, 0x18, 0x19, 0x69, 0x47, 0x71,
    0x61, 0x2c, 0x10, 0x34, 0x79, 0x70, 0x01, 0x3e, 0x54, 0x74, 0x68, 0x0a,
    0x11, 0x04, 0x7f, 0x2c, 0x55, 0x02, 0x10, 0x37, 0x64, 0x3e, 0x0a, 0x41,
    0x6b, 0x0a, 0x38, 0x0f, 0x08, 0x05, 0x30, 0x45, 0x49, 0x1c, 0x65, 0x4e,
    0x0e, 0x37, 0x1d, 0x47, 0x63, 0x73, 0x41, 0x59, 0x00, 0x77, 0x29, 0x78,
    0x43, 0x2d, 0x7d, 0x51, 0x34, 0x5d, 0x66, 0x6a, 0x2d, 0x5e, 0x21, 0x74,
    0x0e, 0x76, 0x40, 0x75, 0x7e, 0x16, 0x6b, 0x2d, 0x78, 0x60, 0x29, 0x07,
    0x0d, 0x45, 0x3b, 0x68, 0x78, 0x67, 0x3f, 0x60, 0x38, 0x15, 0x14, 0x2a,
    0x02, 0x18, 0x7a, 0x4a, 0x05, 0x3b, 0x2c, 0x3b, 0x22, 0x31, 0x09, 0x1e,
    0x01, 0x12, 0x32, 0x13, 0x24, 0x12, 0x3e, 0x0b, 0x30, 0x6d, 0x1d, 0x1a,
    0x68, 0x28, 0x31, 0x41, 0x17, 0x59, 0x1f, 0x2a, 0x6d, 0x56, 0x2e, 0x6f,
    0x62, 0x4a, 0x27, 0x3c, 0x0c, 0x49, 0x62, 0x23, 0x63, 0x3a, 0x62, 0x7e,
    0x2f, 0x39, 0x60, 0x6e, 0x70, 0x6c, 0x11, 0x18, 0x1d, 0x28, 0x08, 0x65,
    0x53, 0x3b, 0x29, 0x4e, 0x27, 0x01, 0x3b, 0x6d, 0x38, 0x5b, 0x6e, 0x28,
    0x4c, 0x7a, 0x60, 0x7d, 0x7e, 0x00, 0x5f, 0x4c, 0x79, 0x00, 0x31, 0x4d,
    0x1a, 0x7b, 0x19, 0x2b, 0x35, 0x64, 0x24, 0x5f, 0x7d, 0x63, 0x61, 0x37,
    0x03, 0x32, 0x50, 0x64, 0x23, 0x23, 0x4b, 0x53, 0x6c, 0x21, 0x2d, 0x2e,
    0x2a, 0x22, 0x31, 0x49, 0x6e, 0x36, 0x56, 0x71, 0x66, 0x49, 0x24, 0x0e,
    0x13, 0x7a, 0x06, 0x2b, 0x79, 0x41, 0x65, 0x49, 0x47, 0x76, 0x0a, 0x59,
    0x0e, 0x16, 0x0b, 0x21, 0x6d, 0x67, 0x0e, 0x23, 0x46, 0x70, 0x06, 0x6b,
    0x40, 0x0a, 0x54, 0x69, 0x50, 0x06, 0x03, 0x7d, 0x1a, 0x7e, 0x5b, 0x51,
    0x26, 0x3a, 0x43, 0x14, 0x1e, 0x7c, 0x09, 0x2a, 0x17, 0x2b, 0x1f, 0x4a,
    0x16, 0x5e, 0x47, 0x2a, 0x47, 0x05, 0x7f, 0x77, 0x51, 0x32, 0x29, 0x5d,
    0x50, 0x75, 0x75, 0x46, 0x69, 0x6b, 0x10, 0x4e, 0x2a, 0x7f, 0x5d, 0x48,
    0x4f, 0x5b, 0x52, 0x4c, 0x66, 0x4f, 0x6e, 0x41, 0x6d, 0x4f, 0x32, 0x59,
    0x13, 0x37, 0x05, 0x58, 0x14, 0x68, 0x4f, 0x56, 0x24, 0x47, 0x1d, 0x0d,
    0x54, 0x5a, 0x2f, 0x04, 0x2a, 0x30, 0x0a, 0x7c, 0x11, 0x3e, 0x52, 0x27,
    0x40, 0x37, 0x34, 0x6d, 0x0a, 0x6f, 0x29, 0x18, 0x6a, 0x05, 0x76, 0x61,
    0x21, 0x53, 0x04, 0x37, 0x02, 0x7d, 0x0c, 0x16, 0x68, 0x53, 0x39, 0x67,
    0x2c, 0x1b, 0x18, 0x75, 0x2b, 0x2e, 0x30, 0x44, 0x1c, 0x64, 0x10, 0x0d,
    0x13, 0x56, 0x2c, 0x07, 0x12, 0x39, 0x5b, 0x34, 0x31, 0x40, 0x7d, 0x2d,
    0x56, 0x17, 0x33, 0x45, 0x2d, 0x69, 0x57, 0x3a, 0x68, 0x52, 0x07, 0x4b,
    0x2e, 0x40, 0x36, 0x0d, 0x67, 0x05, 0x1e, 0x6b, 0x13, 0x55, 0x60, 0x14,
    0x11, 0x04, 0x6e, 0x24, 0x5c, 0x54, 0x56, 0x52, 0x64, 0x42, 0x68, 0x1e,
    0x69, 0x66, 0x7e, 0x02, 0x3a, 0x63, 0x2c, 0x0a, 0x76, 0x5d, 0x17, 0x61,
    0x4e, 0x0f, 0x46, 0x32, 0x00, 0x05, 0x37, 0x70, 0x38, 0x54, 0x31, 0x34,
    0x40, 0x7f, 0x79, 0x46, 0x01, 0x5c, 0x54, 0x00, 0x28, 0x4a, 0x6b, 0x0c,
    0x0a, 0x55, 0x06, 0x29, 0x49, 0x71, 0x44, 0x44, 0x39, 0x2f, 0x10, 0x22,
    0x15, 0x49, 0x7f, 0x22, 0x2a, 0x20, 0x49, 0x0d, 0x59, 0x72, 0x35, 0x7c,
    0x2b, 0x75, 0x7b, 0x5c, 0x48, 0x5d, 0x1a, 0x2f, 0x55, 0x55, 0x7d, 0x0a,
    0x42, 0x35, 0x19, 0x56, 0x44, 0x71, 0x31, 0x5a, 0x58, 0x7a, 0x1d, 0x0f,
    0x3e, 0x6b, 0x79, 0x1e, 0x52, 0x54, 0x50, 0x7b, 0x2e, 0x1a, 0x24, 0x2c,
    0x44, 0x08, 0x06, 0x13, 0x24, 0x28, 0x56, 0x4a, 0x50, 0x62, 0x26, 0x6f,
    0x77, 0x44, 0x1b, 0x07, 0x7e, 0x07, 0x21, 0x16, 0x60, 0x47, 0x5c, 0x25,
    0x06, 0x76, 0x53, 0x52, 0x3a, 0x31, 0x5a, 0x5f, 0x03, 0x4b, 0x75, 0x3c,
    0x34, 0x40, 0x11, 0x16, 0x07, 0x78, 0x47, 0x02, 0x70, 0x32, 0x4b, 0x28,
    0x78, 0x46, 0x13, 0x17, 0x3d, 0x0a, 0x34, 0x6b, 0x70, 0x4c, 0x46, 0x04,
    0x7c, 0x17, 0x56, 0x0c, 0x69, 0x79, 0x02, 0x23, 0x78, 0x26, 0x75, 0x58,
    0x07, 0x71, 0x56, 0x19, 0x51, 0x77, 0x0e, 0x33, 0x56, 0x33, 0x14, 0x23,
    0x04, 0x78, 0x08, 0x63, 0x51, 0x60, 0x25, 0x28, 0x21, 0x40, 0x7e, 0x18,
    0x6b, 0x0a, 0x35, 0x03, 0x5a, 0x7f, 0x0f, 0x64, 0x2e, 0x0e, 0x36, 0x2b,
    0x1b, 0x49, 0x15, 0x53, 0x5b, 0x62, 0x33, 0x07, 0x63, 0x34, 0x4f, 0x01,
    0x4c, 0x0d, 0x4a, 0x7b, 0x6e, 0x31, 0x44, 0x1b, 0x69, 0x3c, 0x5b, 0x38,
    0x56, 0x66, 0x5a, 0x7b, 0x64, 0x06, 0x70, 0x7a, 0x57, 0x7d, 0x73, 0x09,
    0x6b, 0x60, 0x22, 0x07, 0x18, 0x15, 0x69, 0x75, 0x2b, 0x34, 0x13, 0x25,
    0x35, 0x70, 0x2c, 0x48, 0x02, 0x48, 0x6d, 0x60, 0x46, 0x61, 0x59, 0x54,
    0x38, 0x59, 0x27, 0x53, 0x24, 0x61, 0x6a, 0x60, 0x1a, 0x45, 0x11, 0x6f,
    0x4e, 0x4a, 0x2a, 0x14, 0x1d, 0x32, 0x7a, 0x79, 0x5d, 0x48, 0x21, 0x0a,
    0x30, 0x5e, 0x32, 0x78, 0x1d, 0x2d, 0x5e, 0x79, 0x0b, 0x7c, 0x28, 0x5d,
    0x12, 0x78, 0x2d, 0x06, 0x25, 0x37, 0x76, 0x57, 0x78, 0x60, 0x2e, 0x33,
    0x27, 0x60, 0x61, 0x66, 0x52, 0x13, 0x7c, 0x1c, 0x6a, 0x3c, 0x72, 0x49,
    0x42, 0x24, 0x44, 0x27, 0x12, 0x2b, 0x48, 0x34, 0x6a, 0x5e, 0x68, 0x7c,
    0x07, 0x1b, 0x74, 0x44, 0x08, 0x15, 0x3d, 0x24, 0x08, 0x44, 0x59, 0x59,
    0x01, 0x4a, 0x54, 0x67, 0x03, 0x10, 0x69, 0x52, 0x5f, 0x7e, 0x4b, 0x02,
    0x69, 0x1d, 0x60, 0x2c, 0x38, 0x16, 0x3a, 0x25, 0x74, 0x3e, 0x2c, 0x6b,
    0x21, 0x12, 0x03, 0x2c, 0x45, 0x4f, 0x22, 0x59, 0x39, 0x1e, 0x00, 0x55,
    0x08, 0x6e, 0x46, 0x7b, 0x56, 0x1e, 0x75, 0x54, 0x79, 0x53, 0x1a, 0x54,
    0x4d, 0x00, 0x01, 0x73, 0x73, 0x20, 0x33, 0x62, 0x1f, 0x6d, 0x52, 0x19,
    0x3b, 0x56, 0x16, 0x16, 0x21, 0x5f, 0x6b, 0x5f, 0x6c, 0x0e, 0x68, 0x49,
    0x22, 0x4c, 0x01, 0x14, 0x6f, 0x4c, 0x5a, 0x19, 0x00, 0x3c, 0x0f, 0x6c,
    0x5f, 0x42, 0x72, 0x6a, 0x1f, 0x17, 0x2d, 0x51, 0x4f, 0x0c, 0x5b, 0x29,
    0x0a, 0x64, 0x0a, 0x1f, 0x2f, 0x2a, 0x2d, 0x57, 0x5e, 0x76, 0x5d, 0x7f,
    0x09, 0x56, 0x76, 0x3e, 0x5c, 0x2c, 0x2d, 0x50, 0x08, 0x12, 0x28, 0x26,
    0x5d, 0x07, 0x6f, 0x03, 0x20, 0x40, 0x79, 0x4c, 0x5f, 0x3d, 0x35, 0x1b,
    0x45, 0x1a, 0x7a, 0x54, 0x3f, 0x4e, 0x46, 0x7e, 0x70, 0x43, 0x0e, 0x61,
    0x1b, 0x65, 0x15, 0x18, 0x35, 0x59, 0x55, 0x74, 0x12, 0x63, 0x35, 0x48,
    0x61, 0x14, 0x3b, 0x4a, 0x67, 0x73, 0x13, 0x05, 0x04, 0x10, 0x5a, 0x4c,
    0x32, 0x08, 0x51, 0x10, 0x69, 0x3e, 0x32, 0x5d, 0x10, 0x4c, 0x30, 0x13,
    0x4b, 0x4b, 0x4c, 0x2a, 0x24, 0x57, 0x59, 0x76, 0x2c, 0x7c, 0x71, 0x58,
    0x29, 0x0b, 0x75, 0x47, 0x57, 0x1b, 0x51, 0x64, 0x67, 0x68, 0x2c, 0x0d,
    0x18, 0x7b, 0x21, 0x39, 0x4a, 0x11, 0x4c, 0x63, 0x59, 0x0f, 0x28, 0x75,
    0x5d, 0x6f, 0x3f, 0x56, 0x08, 0x3c, 0x7b, 0x31, 0x2b, 0x13, 0x40, 0x7d,
    0x6b, 0x41, 0x45, 0x73, 0x04, 0x6a, 0x43, 0x68, 0x15, 0x10, 0x64, 0x70,
    0x3a, 0x5a, 0x14, 0x0c, 0x3e, 0x77, 0x6e, 0x22, 0x69, 0x20, 0x30, 0x0c,
    0x59, 0x20, 0x3f, 0x18, 0x43, 0x77, 0x3f, 0x34, 0x29, 0x60, 0x23, 0x07,
    0x44, 0x45, 0x1f, 0x4b, 0x0f, 0x29, 0x75, 0x51, 0x0d, 0x5a, 0x07, 0x70,
    0x39, 0x26, 0x0e, 0x07, 0x6a, 0x50, 0x7c, 0x6e, 0x0d, 0x3e, 0x5a, 0x1e,
    0x4d, 0x61, 0x0c, 0x21, 0x26, 0x2d, 0x14, 0x40, 0x45, 0x07, 0x24, 0x35,
    0x4b, 0x5e, 0x48, 0x26, 0x04, 0x02, 0x2d, 0x55, 0x1a, 0x6b, 0x0f, 0x56,
    0x48, 0x17, 0x38, 0x00, 0x35, 0x75, 0x46, 0x4c, 0x75, 0x5e, 0x65, 0x18,
    0x64, 0x79, 0x6e, 0x2b, 0x38, 0x04, 0x0e, 0x6b, 0x2d, 0x68, 0x60, 0x02,
    0x02, 0x2c, 0x4a, 0x57, 0x54, 0x06, 0x20, 0x08, 0x17, 0x72, 0x3b, 0x63,
    0x51, 0x67, 0x38, 0x0f, 0x5c, 0x0d, 0x0b, 0x7c, 0x09, 0x08, 0x34, 0x58,
    0x42, 0x64, 0x54, 0x6e, 0x17, 0x7a, 0x43, 0x54, 0x19, 0x65, 0x4f, 0x6d,
    0x33, 0x20, 0x54, 0x40, 0x42, 0x33, 0x39, 0x6d, 0x15, 0x3b, 0x74, 0x13,
    0x5b, 0x1a, 0x16, 0x6f, 0x5e, 0x3c, 0x57, 0x05, 0x42, 0x08, 0x37, 0x4a,
    0x5a, 0x76, 0x00, 0x15, 0x52, 0x1b, 0x4a, 0x4e, 0x73, 0x3a, 0x3a, 0x3f,
    0x41, 0x6d, 0x1d, 0x2f, 0x55, 0x5d, 0x54, 0x50, 0x1b, 0x39, 0x53, 0x7d,
    0x52, 0x51, 0x02, 0x01, 0x4b, 0x01, 0x47, 0x63, 0x7c, 0x48, 0x0b, 0x4c,
    0x6a, 0x02, 0x18, 0x71, 0x78, 0x43, 0x26, 0x49, 0x18, 0x0f, 0x01, 0x2a,
    0x6e, 0x7c, 0x4e, 0x10, 0x50, 0x72, 0x11, 0x02, 0x3c, 0x62, 0x2a, 0x15,
    0x7d, 0x6b, 0x10, 0x19, 0x09, 0x1b, 0x32, 0x24, 0x4f, 0x53, 0x0c, 0x6f,
    0x70, 0x21, 0x5d, 0x2b, 0x68, 0x03, 0x68, 0x68, 0x5c, 0x0c, 0x28, 0x29,
    0x42, 0x71, 0x01, 0x0e, 0x22, 0x07, 0x21, 0x3a, 0x0f, 0x57, 0x39, 0x76,
    0x41, 0x34, 0x67, 0x58, 0x13, 0x3b, 0x61, 0x77, 0x0e, 0x71, 0x65, 0x23,
    0x40, 0x7a, 0x0a, 0x4d, 0x44, 0x09, 0x23, 0x28, 0x7d, 0x22, 0x7c, 0x05,
    0x40, 0x42, 0x7b, 0x47, 0x44, 0x10, 0x51, 0x22, 0x51, 0x2b, 0x23, 0x2d,
    0x20, 0x31, 0x22, 0x29, 0x2e, 0x2e, 0x3e, 0x70, 0x7f, 0x38, 0x77, 0x4b,
    0x2f, 0x35, 0x66, 0x32, 0x57, 0x4f, 0x62, 0x3d, 0x14, 0x77, 0x69, 0x6e,
    0x05, 0x7d, 0x09, 0x71, 0x60, 0x43, 0x19, 0x50, 0x60, 0x75, 0x4f, 0x07,
    0x16, 0x0f, 0x44, 0x45, 0x01, 0x18, 0x38, 0x46, 0x3d, 0x1a, 0x51, 0x5f,
    0x08, 0x0e, 0x5b, 0x59, 0x2c, 0x39, 0x69, 0x39, 0x70, 0x52, 0x21, 0x2d,
    0x2f, 0x27, 0x07, 0x33, 0x20, 0x77, 0x0a, 0x46, 0x21, 0x53, 0x6e, 0x61,
    0x52, 0x07, 0x51, 0x68, 0x02, 0x11, 0x14, 0x3e, 0x30, 0x32, 0x37, 0x19,
    0x73, 0x29, 0x75, 0x49, 0x03, 0x66, 0x39, 0x7b, 0x0c, 0x45, 0x5c, 0x53,
    0x75, 0x65, 0x79, 0x38, 0x06, 0x3d, 0x41, 0x0a, 0x2e, 0x34, 0x3f, 0x1b,
    0x5a, 0x02, 0x34, 0x5c, 0x5f, 0x09, 0x27, 0x71, 0x4a, 0x20, 0x7b, 0x21,
    0x40, 0x56, 0x5f, 0x06, 0x58, 0x06, 0x6e, 0x06, 0x4f, 0x24, 0x2e, 0x16,
    0x04, 0x05, 0x1d, 0x2e, 0x0f, 0x40, 0x57, 0x74, 0x74, 0x26, 0x4a, 0x70,
    0x0e, 0x1f, 0x1d, 0x19, 0x15, 0x71, 0x59, 0x2a, 0x6e, 0x47, 0x01, 0x32,
    0x5f, 0x2d, 0x10, 0x20, 0x12, 0x6f, 0x43, 0x65, 0x2f, 0x71, 0x3b, 0x69,
    0x7e, 0x22, 0x24, 0x4b, 0x44, 0x0f, 0x37, 0x0a, 0x25, 0x49, 0x02, 0x53,
    0x18, 0x72, 0x24, 0x4c, 0x52, 0x7d, 0x0f, 0x26, 0x3f, 0x11, 0x36, 0x30,
    0x65, 0x62, 0x01, 0x38, 0x24, 0x67, 0x09, 0x2f, 0x73, 0x01, 0x1b, 0x37,
    0x36, 0x29, 0x0f, 0x69, 0x5e, 0x3c, 0x13, 0x37, 0x4f, 0x1e, 0x6e, 0x39,
    0x09, 0x10, 0x11, 0x5b, 0x28, 0x7d, 0x77, 0x0c, 0x78, 0x2c, 0x40, 0x0c,
    0x58, 0x2b, 0x30, 0x56, 0x04, 0x4d, 0x5f, 0x48, 0x45, 0x03, 0x7a, 0x53,
    0x44, 0x6c, 0x12, 0x38, 0x0a, 0x05, 0x5f, 0x49, 0x4d, 0x72, 0x61, 0x77,
    0x2a, 0x6a, 0x21, 0x77, 0x0d, 0x24, 0x40, 0x06, 0x52, 0x0e, 0x03, 0x50,
    0x11, 0x4b, 0x4e, 0x17, 0x4f, 0x0f, 0x51, 0x16, 0x10, 0x6f, 0x22, 0x1f,
    0x0e, 0x49, 0x44, 0x18, 0x6b, 0x15, 0x64, 0x3c, 0x53, 0x60, 0x22, 0x1a,
    0x71, 0x12, 0x4b, 0x47, 0x7f, 0x4e, 0x0f, 0x3a, 0x4c, 0x5d, 0x21, 0x18,
    0x58, 0x30, 0x24, 0x05, 0x17, 0x2d, 0x74, 0x50, 0x75, 0x70, 0x74, 0x27,
    0x1d, 0x60, 0x5d, 0x39, 0x6c, 0x30, 0x0c, 0x0c, 0x3a, 0x54, 0x4d, 0x20,
    0x71, 0x68, 0x18, 0x1d, 0x3c, 0x40, 0x57, 0x48, 0x7d, 0x37, 0x07, 0x25,
    0x1c, 0x19, 0x21, 0x7f, 0x11, 0x44, 0x33, 0x3e, 0x10, 0x4a, 0x42, 0x12,
    0x77, 0x58, 0x6d, 0x37, 0x74, 0x36, 0x30, 0x63, 0x39, 0x18, 0x37, 0x0f,
    0x14, 0x02, 0x32, 0x12, 0x5e, 0x34, 0x4a, 0x50, 0x37, 0x2c, 0x55, 0x26,
    0x15, 0x04, 0x5a, 0x5c, 0x57, 0x40, 0x2f, 0x0a, 0x4a, 0x3f, 0x3f, 0x4c,
    0x17, 0x2c, 0x31, 0x11, 0x28, 0x48, 0x5d, 0x71, 0x11, 0x76, 0x02, 0x1b,
    0x39, 0x34, 0x6c, 0x10, 0x1a, 0x74, 0x20, 0x7b, 0x3c, 0x28, 0x0c, 0x63,
    0x07, 0x29, 0x56, 0x25, 0x7e, 0x15, 0x2b, 0x0e, 0x69, 0x35, 0x7a, 0x1f,
    0x32, 0x36, 0x09, 0x21, 0x23, 0x5b, 0x79, 0x3b, 0x73, 0x50, 0x03, 0x67,
    0x6d, 0x5d, 0x26, 0x1d, 0x37, 0x6f, 0x2f, 0x4a, 0x3a, 0x49, 0x11, 0x0c,
    0x6b, 0x16, 0x66, 0x2e, 0x24, 0x2a, 0x37, 0x1f, 0x30, 0x04, 0x53, 0x3e,
    0x20, 0x79, 0x56, 0x69, 0x5c, 0x36, 0x5c, 0x52, 0x05, 0x12, 0x68, 0x41,
    0x15, 0x06, 0x69, 0x19, 0x46, 0x2e, 0x6a, 0x03, 0x77, 0x66, 0x65, 0x01,
    0x12, 0x33, 0x66, 0x02, 0x4b, 0x7e, 0x29, 0x64, 0x45, 0x01, 0x18, 0x46,
    0x5f, 0x6b, 0x0e, 0x3f, 0x3a, 0x16, 0x75, 0x19, 0x7d, 0x79, 0x64, 0x61,
    0x51, 0x04, 0x53, 0x2b, 0x7c, 0x57, 0x23, 0x46, 0x66, 0x25, 0x2f, 0x33,
    0x43, 0x47, 0x70, 0x7a, 0x38, 0x5b, 0x05, 0x25, 0x64, 0x1b, 0x4c, 0x33,
    0x12, 0x76, 0x2a, 0x38, 0x57, 0x36, 0x1d, 0x7f, 0x1d, 0x79, 0x43, 0x4f,
    0x7c, 0x4a, 0x43, 0x43, 0x0e, 0x06, 0x6f, 0x57, 0x5c, 0x45, 0x4d, 0x75,
    0x63, 0x31, 0x36, 0x30, 0x2b, 0x22, 0x22, 0x6f, 0x09, 0x38, 0x63, 0x39,
    0x48, 0x63, 0x74, 0x63, 0x34, 0x7c, 0x0b, 0x01, 0x69, 0x6f, 0x44, 0x73,
    0x40, 0x02, 0x4a, 0x61, 0x2d, 0x6f, 0x4d, 0x2f, 0x4d, 0x73, 0x7f, 0x08,
    0x52, 0x16, 0x6f, 0x3b, 0x34, 0x56, 0x3c, 0x54, 0x2a, 0x43, 0x46, 0x16,
    0x46, 0x14, 0x17, 0x3d, 0x41, 0x29, 0x0d, 0x0e, 0x01, 0x79, 0x7a, 0x48,
    0x11, 0x56, 0x3e, 0x32, 0x40, 0x12, 0x5a, 0x01, 0x54, 0x0f, 0x59, 0x33,
    0x7c, 0x03, 0x3a, 0x40, 0x41, 0x6a, 0x4c, 0x4c, 0x6b, 0x0f, 0x5a, 0x5f,
    0x38, 0x54, 0x14, 0x20, 0x67, 0x09, 0x19, 0x15, 0x7b, 0x19, 0x68, 0x20,
    0x03, 0x34, 0x26, 0x5f, 0x35, 0x79, 0x6c, 0x3f, 0x36, 0x75, 0x0a, 0x06,
    0x7f, 0x01, 0x7e, 0x33, 0x15, 0x4d, 0x2e, 0x25, 0x3c, 0x22, 0x53, 0x7c,
    0x57, 0x30, 0x2c, 0x15, 0x6c, 0x0a, 0x6c, 0x7b, 0x4e, 0x21, 0x74, 0x29,
    0x22, 0x1b, 0x3e, 0x07, 0x55, 0x7c, 0x25, 0x14, 0x7d, 0x5f, 0x44, 0x4f,
    0x6c, 0x5e, 0x5b, 0x74, 0x03, 0x6a, 0x58, 0x35, 0x79, 0x5d, 0x2c, 0x5d,
    0x4c, 0x47, 0x10, 0x1d, 0x5a, 0x26, 0x20, 0x7b, 0x47, 0x26, 0x3d, 0x15,
    0x78, 0x1f, 0x13, 0x51, 0x02, 0x32, 0x27, 0x14, 0x4e, 0x3e, 0x17, 0x3a,
    0x74, 0x00, 0x2e, 0x7f, 0x2c, 0x2e, 0x4f, 0x72, 0x63, 0x7e, 0x28, 0x09,
    0x2b, 0x37, 0x7c, 0x7a, 0x15, 0x7f, 0x7b, 0x6d, 0x0c, 0x00, 0x05, 0x62,
    0x46, 0x6f, 0x49, 0x03, 0x42, 0x62, 0x2a, 0x0e, 0x19, 0x07, 0x59, 0x15,
    0x30, 0x0c, 0x4f, 0x4f, 0x3d, 0x26, 0x6d, 0x6a, 0x74, 0x01, 0x48, 0x3b,
    0x36, 0x11, 0x2c, 0x60, 0x51, 0x4b, 0x16, 0x7a, 0x38, 0x5f, 0x64, 0x15,
    0x11, 0x68, 0x08, 0x63, 0x6d, 0x4e, 0x28, 0x7a, 0x4f, 0x7b, 0x6a, 0x7f,
    0x21, 0x6a, 0x7a, 0x6e, 0x39, 0x38, 0x37, 0x4f, 0x7e, 0x5a, 0x20, 0x49,
    0x75, 0x6c, 0x67, 0x5c, 0x06, 0x78, 0x14, 0x1f, 0x4b, 0x17, 0x12, 0x06,
    0x34, 0x18, 0x20, 0x4f, 0x3b, 0x47, 0x6b, 0x1b, 0x7d, 0x79, 0x2a, 0x4c,
    0x02, 0x62, 0x45, 0x40, 0x03, 0x75, 0x6d, 0x06, 0x08, 0x52, 0x57, 0x5c,
    0x2e, 0x43, 0x21, 0x04, 0x73, 0x79, 0x52, 0x2a, 0x2f, 0x06, 0x3e, 0x78,
    0x6b, 0x67, 0x68, 0x62, 0x63, 0x19, 0x21, 0x2d, 0x58, 0x54, 0x18, 0x40,
    0x32, 0x71, 0x16, 0x62, 0x10, 0x6f, 0x1f, 0x1f, 0x48, 0x13, 0x13, 0x63,
    0x1b, 0x52, 0x55, 0x40, 0x58, 0x5c, 0x4a, 0x60, 0x31, 0x39, 0x04, 0x2d,
    0x79, 0x1a, 0x3a, 0x37, 0x4a, 0x2a, 0x35, 0x36, 0x0a, 0x18, 0x60, 0x38,
    0x73, 0x51, 0x05, 0x57, 0x58, 0x56, 0x4a, 0x14, 0x27, 0x2a, 0x02, 0x59,
    0x77, 0x6a, 0x08, 0x04, 0x49, 0x73, 0x6c, 0x01, 0x7c, 0x01, 0x21, 0x7d,
    0x3f, 0x78, 0x41, 0x08, 0x5a, 0x07, 0x28, 0x03, 0x7a, 0x73, 0x78, 0x37,
    0x65, 0x2a, 0x62, 0x1b, 0x7b, 0x32, 0x18, 0x59, 0x4c, 0x53, 0x79, 0x1e,
    0x27, 0x77, 0x56, 0x7b, 0x2f, 0x26, 0x45, 0x17, 0x53, 0x71, 0x09, 0x45,
    0x22, 0x39, 0x67, 0x29, 0x36, 0x24, 0x40, 0x67, 0x08, 0x06, 0x17, 0x71,
    0x56, 0x41, 0x19, 0x2f, 0x01, 0x75, 0x79, 0x21, 0x38, 0x51, 0x3c, 0x5d,
    0x6f, 0x79, 0x05, 0x1c, 0x79, 0x4f, 0x29, 0x01, 0x6d, 0x5a, 0x31, 0x3a,
    0x75, 0x2c, 0x11, 0x39, 0x4b, 0x3b, 0x73, 0x43, 0x10, 0x2d, 0x6d, 0x65,
    0x3a, 0x36, 0x6c, 0x42, 0x67, 0x1b, 0x17, 0x71, 0x1b, 0x42, 0x2e, 0x40,
    0x2f, 0x25, 0x71, 0x1d, 0x62, 0x1b, 0x68, 0x5d, 0x1e, 0x03, 0x41, 0x50,
    0x01, 0x7b, 0x00, 0x79, 0x0f, 0x34, 0x65, 0x31, 0x70, 0x6b, 0x47, 0x64,
    0x13, 0x0e, 0x53, 0x0d, 0x34, 0x0d, 0x33, 0x55, 0x78, 0x75, 0x5f, 0x3e,
    0x3d, 0x3d, 0x68, 0x3a, 0x1b, 0x47, 0x50, 0x0e, 0x63, 0x38, 0x48, 0x49,
    0x17, 0x47, 0x7d, 0x5b, 0x65, 0x23, 0x30, 0x0b, 0x46, 0x75, 0x50, 0x75,
    0x48, 0x04, 0x3b, 0x76, 0x63, 0x3c, 0x6e, 0x27, 0x05, 0x2e, 0x66, 0x14,
    0x24, 0x41, 0x20, 0x76, 0x32, 0x0f, 0x4d, 0x06, 0x57, 0x66, 0x7e, 0x27,
    0x60, 0x10, 0x6f, 0x2a, 0x65, 0x52, 0x0d, 0x55, 0x14, 0x7c, 0x65, 0x12,
    0x57, 0x36, 0x68, 0x4e, 0x2e, 0x53, 0x4c, 0x58, 0x26, 0x32, 0x79, 0x51,
    0x18, 0x06, 0x58, 0x75, 0x4a, 0x5d, 0x56, 0x18, 0x08, 0x58, 0x7a, 0x09,
    0x17, 0x61, 0x62, 0x18, 0x7d, 0x77, 0x61, 0x1c, 0x72, 0x16, 0x0c, 0x50,
    0x6e, 0x60, 0x41, 0x61, 0x02, 0x25, 0x3b, 0x4b, 0x0d, 0x58, 0x44, 0x6c,
    0x5c, 0x0d, 0x60, 0x2e, 0x0b, 0x7a, 0x58, 0x73, 0x2e, 0x4a, 0x74, 0x63,
    0x2c, 0x4a, 0x42, 0x20, 0x2c, 0x7f, 0x46, 0x1a, 0x09, 0x6a, 0x33, 0x2f,
    0x63, 0x0e, 0x64, 0x50, 0x01, 0x18, 0x0f, 0x6c, 0x60, 0x1b, 0x52, 0x06,
    0x53, 0x1e, 0x17, 0x07, 0x42, 0x3c, 0x21, 0x5d, 0x11, 0x44, 0x28, 0x7f,
    0x07, 0x6f, 0x2d, 0x79, 0x4a, 0x7f, 0x0f, 0x56, 0x78, 0x6d, 0x3c, 0x63,
    0x00, 0x59, 0x7c, 0x09, 0x47, 0x2e, 0x25, 0x6f, 0x1a, 0x02, 0x1b, 0x54,
    0x4c, 0x2b, 0x35, 0x28, 0x07, 0x78, 0x18, 0x0a, 0x06, 0x63, 0x18, 0x5f,
    0x71, 0x78, 0x74, 0x5d, 0x3d, 0x3c, 0x57, 0x32, 0x27, 0x34, 0x6e, 0x52,
    0x30, 0x0a, 0x21, 0x62, 0x03, 0x46, 0x03, 0x0f, 0x7a, 0x7c, 0x14, 0x22,
    0x58, 0x7b, 0x37, 0x44, 0x3a, 0x7a, 0x7d, 0x06, 0x55, 0x4e, 0x1b, 0x55,
    0x28, 0x65, 0x40, 0x26, 0x0a, 0x51, 0x50, 0x7a, 0x2d, 0x43, 0x60, 0x75,
    0x0e, 0x5b, 0x64, 0x17, 0x7d, 0x57, 0x28, 0x52, 0x65, 0x5b, 0x31, 0x0f,
    0x06, 0x45, 0x5f, 0x51, 0x75, 0x33, 0x36, 0x63, 0x6a, 0x04, 0x79, 0x33,
    0x6d, 0x4e, 0x49, 0x0e, 0x70, 0x45, 0x77, 0x24, 0x1e, 0x50, 0x1b, 0x15,
    0x13, 0x62, 0x50, 0x2a};
#define MAX_RAND_IDX ((sizeof(th_rand_data) / sizeof(int)) - 1)

/* Function: th_rand
        A thread safe pseudo random number generator.
        Must initialize with th_srand.

        Params:
                id - Return value from a previous th_srand call.
                        Each thread must initialize with th_srand.

        Returns:
                A pseudo random number, that depends on id, and is guaranteed
                to produce the same sequence at every run
*/
int th_rand(void *id) {
  int *pidx = (int *)id;
  int idx = *pidx;
  if (++idx > MAX_RAND_IDX)
    idx = 0;
  *pidx = idx;
  return th_rand_data[idx];
}
/* Function: th_srand
        Initialize a thread safe pseudo random number generator.

        Params:
                seed - different seed numbers will lead to different
                        pseudo random number sequences.

        Returns:
                An id that should be passed to subsequent
                th_rand calls by that thread.

        Note:
                Call th_frand before thread exits to clean up.
*/
void *th_srand(unsigned int seed) {
  int *pid = (int *)th_malloc(sizeof(int));
  if (pid == NULL) {
    th_log(0, "ERROR: failed malloc!");
    return 0;
  }
  *pid = seed % MAX_RAND_IDX;
  return pid;
}
/* */
unsigned int th_randseed(void *curid) {
  int *seed = (int *)curid;
  return *seed;
}
/* Function: th_frand
        Clean up data that was previosuly allocated by a call to <th_srand>
*/
void th_frand(void *id) {
  if (id != 0)
    th_free(id);
}

#if !HAVE_ASSERT_H
/*------------------------------------------------------------------------------
 * function: __assertfail
 *
 * Description:
 *			Called by the assert() macro when a condition fails
 *
 * Note:
 *			Does not return!
 * ---------------------------------------------------------------------------*/

void __assertfail(const char *msg, const char *cond, const char *file, int line)

{
  th_printf("message: %s\nCondition: %s\nFile: %s at %d\n", msg, cond, file,
            line);
  al_exit(THE_FAILURE);
}

/*------------------------------------------------------------------------------
 * function: __fatal
 *
 * Description:
 *			called when a fatal error is encountered
 * ---------------------------------------------------------------------------*/

void __fatal(const char *msg, const char *file, int line)

{
  th_printf("message: %s\nFile: %s at %d\n", msg, file, line);
  al_exit(THE_FAILURE);
}
#endif

#if REPORT_THMALLOC_STATS
void init_thmalloc_stats() {
  al_mutex_init(&th_malloc_mutex);
  al_mutex_lock(&th_malloc_mutex);
  th_malloc_total = 0;
  ee_list_add(&th_malloc_sizes, NULL, 0); /* dummy header for the list*/
  al_mutex_unlock(&th_malloc_mutex);
}
#endif
/* Convenience Functions  */
/* ******************** */

/* ee_list */

ee_intlist *ee_list_add(ee_intlist **p, void *data, unsigned int info) {
  /* some compilers don't require a cast of return value for malloc */
  ee_intlist *n = (ee_intlist *)malloc(sizeof(ee_intlist));
  if (n == NULL)
    return NULL;

  n->next = *p; /* the previous element (*p) now becomes the "next" element */
  *p = n;       /* add new empty element to the front (head) of the list */
  n->data = data;
  n->info = info;

  return *p;
}
void ee_list_remove(ee_intlist **p) /* remove head */
{
  if (*p != NULL) {
    ee_intlist *n = *p;
    *p = (*p)->next;
    free(n);
  }
}
ee_intlist **ee_list_isearch(ee_intlist **n, unsigned int info) {
  while (*n != NULL) {
    if ((*n)->info == info) {
      return n;
    }
    n = &(*n)->next;
  }
  return NULL;
}
ee_intlist **ee_list_dsearch(ee_intlist **n, void *data) {
  while (*n != NULL) {
    if ((*n)->data == data) {
      return n;
    }
    n = &(*n)->next;
  }
  return NULL;
}

#define out_malloc(_p, _s)                                                     \
  if (_p) {                                                                    \
    *_p = (_s *)th_malloc(((*numread) + 1) * sizeof(_s));                      \
    if (*_p == NULL)                                                           \
      th_exit(THE_OUT_OF_MEMORY, "Cannot Allocate Memory %s : %d", __FILE__,   \
              __LINE__);                                                       \
  }

/* Automotive common functions */
void get_auto_data(char *fname, e_s32 **outi, e_f64 **outf, e_u8 **outb,
                   int *numread) {
  ee_FILE *fd;
  n_int i = 0, j = 0;
  e_u8 buf[50];
  n_int ch;
  if ((fd = th_fopen(fname, "rb")) == NULL)
    th_exit(THE_FAILURE, "Failure: Cannot open file '%s'\n", fname);

  /* first thing in the file is an int specifying max number of items to read */
  ch = th_getc(fd);
  while ((ch != '\r') && (ch != '\n')) {
    buf[i++] = (e_u8)ch;
    ch = th_getc(fd);
  }
  buf[i] = '\0';
  *numread = th_atoi((char *)buf);

  out_malloc(outi, e_s32);
  out_malloc(outf, e_f64);
  out_malloc(outb, e_u8);
  while (((ch = th_getc(fd)) != EOF) && (j < *numread)) {
    i = 0;

    while ((ch != '\t') && (ch != '\r') && (ch != ',') && (ch != '\n') &&
           (ch != ' ') && (ch != EOF)) {

      buf[i++] = (e_u8)ch;
      ch = th_getc(fd);
    }
    buf[i] = '\0';
    if (buf[0] != '\0') {
      if (outi)
        (*outi)[j++] = (e_s32)th_atoi((char *)buf);
      if (outb)
        (*outb)[j++] = (e_u8)th_atoi((char *)buf);
      if (outf)
        (*outf)[j++] = (e_f64)th_atof((char *)buf);
    }
  }
  th_fclose(fd);
  /* return actual number of items read */
  *numread = j;
}

void get_auto_data_int(char *fname, e_s32 **out, int *numread) {
  get_auto_data(fname, out, NULL, NULL, numread);
}
void get_auto_data_byte(char *fname, e_u8 **out, int *numread) {
  get_auto_data(fname, NULL, NULL, out, numread);
}
void get_auto_data_dbl(char *fname, e_f64 **out, int *numread) {
  get_auto_data(fname, NULL, out, NULL, numread);
}
