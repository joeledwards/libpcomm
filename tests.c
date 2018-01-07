#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "pcomm.h"
#include "simclist.h"

#define NANOSECOND        (int64_t)1LL
#define MICROSECOND    (int64_t)1000LL
#define MILLISECOND (int64_t)1000000LL
#define SECOND   (int64_t)1000000000LL
#define MINUTE  (int64_t)60000000000LL
#define HOUR  (int64_t)3600000000000LL

#define COLOR_OFF     "\x1B[0m"
#define COLOR_RED     "\x1B[31m"
#define COLOR_GREEN   "\x1B[32m"
#define COLOR_YELLOW  "\x1B[33m"
#define COLOR_BLUE    "\x1B[34m"
#define COLOR_MAGENTA "\x1B[35m"
#define COLOR_CYAN    "\x1B[36m"
#define COLOR_WHITE   "\x1B[37m"

// Represents the current date+time at nanosecond granularity.
struct tm_ext {
  struct tm dateTime;
  int64_t nanos;
};

// Supplies the current time in nanoseconds.
int64_t get_nano_timestamp(void) {
  struct timespec spec;
  int64_t nanoTs;

  clock_gettime(CLOCK_REALTIME, &spec);
  nanoTs = SECOND * (int64_t)(spec.tv_sec) + (int64_t)(spec.tv_nsec);

  return nanoTs;
}

// Converts a nanosecond timestamp to struct tm_ext.
void ns_to_utc_date_time(int64_t nanoTs, struct tm_ext *timeInfo) {
  time_t timestamp;
  struct tm *dateTime;

  timestamp = (time_t)(nanoTs / SECOND);
  dateTime = gmtime(&timestamp);

  memcpy(&(timeInfo->dateTime), dateTime, sizeof(struct tm));
  timeInfo->nanos = nanoTs % SECOND;
}

// Supplies the current time as a struct tm_ext.
void get_utc_date_time(struct tm_ext *timeInfo) {
  int64_t nanoTs = get_nano_timestamp();
  ns_to_utc_date_time(nanoTs, timeInfo);
}

// Fills out a struct tm with the current date+time.
void get_date_time(struct tm *dateTime) {
  time_t timestamp;
  struct tm *dt;
  
  timestamp = time(NULL);
  dt = localtime(&timestamp);

  memcpy(dateTime, dt, sizeof(struct tm));
}

// Prints the duration represented by a number of nanoseconds.
void print_nano_duration(int64_t nanos) {
  int64_t high = 0;
  int64_t low = 0;
  int negative = nanos < (int64_t)0 ? 1 : 0;
  nanos = negative ? ((int64_t)-1 * nanos) : nanos;

  if (nanos >= HOUR) {
    high = nanos / HOUR;
    low = nanos % HOUR / MINUTE;
    printf("%lld h, %lld m\n", high, low);
  } else if (nanos >= MINUTE) {
    high = nanos / MINUTE;
    low = nanos % MINUTE / SECOND;
    printf("%lld m, %lld s\n", high, low);
  } else if (nanos >= SECOND) {
    high = nanos / SECOND;
    low = nanos % SECOND / MILLISECOND;
    printf("%lld.%03lld s\n", high, low);
  } else if (nanos >= MILLISECOND) {
    high = nanos / MILLISECOND;
    low = nanos % MILLISECOND / MICROSECOND;
    printf("%lld.%03lld ms\n", high, low);
  } else {
    high = nanos / MICROSECOND;
    low = nanos % MICROSECOND;
    printf("%lld.%03lld us\n", high, low);
  }
}

// Used to track the progress of tests.
struct test_context {
  int passed;
  int failed;
  char *group;
};

// Update the test group.
void group(struct test_context *context, char *group) {
  context->group = group;
}

// Updates the context with the result of a single test.
void test(struct test_context *context, char *description, int assertion) {
  char *group;

  if (assertion) {
    context->passed++;
  } else {
    context->failed++;
  }

  group = (context->group == NULL) ? "test" : context->group;

  printf(
    "[%s%s%s] %s [%s%s%s]\n",
    COLOR_BLUE,
    group,
    COLOR_OFF,
    description,
    assertion ? COLOR_GREEN : COLOR_RED,
    assertion ? "OK" : "FAIL",
    COLOR_OFF
  );
}

void test_init(struct test_context *t) {
  pcomm_context_t context;
  pcomm_context_t *c = &context;

  group(t, "initialize context");

  pcomm_init(c);

  test(t, "zero read fds post init", list_size(&c->read_fds) == 0);
  test(t, "zero write fds post init", list_size(&c->write_fds) == 0);
  test(t, "zero error fds post init", list_size(&c->error_fds) == 0);
  test(t, "marked as initialized post init", c->initialized == 1);
  test(t, "page size is 4096 post init", c->page_size == 4096);
  test(t, "external context should be NULL post init", c->external_context == NULL);
  test(t, "prepare_callback should be NULL post init", c->prepare_callback == NULL);
  test(t, "select_callback should be NULL post init", c->select_callback == NULL);
  test(t, "timeout_callback should be NULL post init", c->timeout_callback == NULL);
  test(t, "timeout seconds should be zero post init", c->timeout.tv_sec == 0);
  test(t, "timeout microseconds should be zero post init", c->timeout.tv_usec == 0);
  test(t, "debug should be disabled post init", c->debug == 0);
  test(t, "exit_now should be false post init", c->exit_now == 0);
  test(t, "exit_request should be false post init", c->exit_request == 0);

  group(t, NULL);
}

void test_destroy(struct test_context *t) {
  pcomm_context_t context;
  pcomm_context_t *c = &context;

  group(t, "destroy context");

  pcomm_init(c);
  pcomm_destroy(c);

  test(t, "not marked as initialized post destroy", c->initialized == 0);
  test(t, "external context should be NULL post destroy", c->external_context == NULL);
  test(t, "prepare_callback should be NULL post destroy", c->prepare_callback == NULL);
  test(t, "select_callback should be NULL post destroy", c->select_callback == NULL);
  test(t, "timeout_callback should be NULL post destroy", c->timeout_callback == NULL);

  group(t, NULL);
}

void test_external_context(struct test_context *t) {
  pcomm_context_t context;
  pcomm_context_t *c = &context;

  group(t, "external context");

  pcomm_init(c);
  test(t, "external context should be NULL before set", c->external_context == NULL);

  pcomm_set_external_context(c, (void *) t);
  test(t, "external context should not be NULL after set", c->external_context != NULL);
  test(t, "external context should be supplied when requested",
      pcomm_get_external_context(c) == (void *)t);

  group(t, NULL);
}

void test_debug_mode(struct test_context *t) {
  pcomm_context_t context;
  pcomm_context_t *c = &context;

  group(t, "debug mode");

  pcomm_init(c);
  test(t, "debug should be disabled by default", pcomm_get_debug(c) == 0);

  pcomm_set_debug(c, 1);
  test(t, "debug should be enabled by value 1", pcomm_get_debug(c) == 1);

  pcomm_set_debug(c, 0);
  test(t, "debug should be disabled by value 0", pcomm_get_debug(c) == 0);

  pcomm_set_debug(c, 256);
  test(t, "debug should be enabled by positive non-zero value", pcomm_get_debug(c) == 1);

  pcomm_set_debug(c, 0);
  pcomm_set_debug(c, -256);
  test(t, "debug should be enabled by negative non-zero value", pcomm_get_debug(c) == 1);

  group(t, NULL);
}

void test_debug_mode(struct test_context *t) {
  pcomm_context_t context;
  pcomm_context_t *c = &context;

  group(t, "debug mode");

  pcomm_init(c);

  group(t, NULL);
}

// Run through each of the test group functions.
void run_tests(struct test_context *t) {
  group(t, "start");
  test(t, "tests-started", t->passed == 0);
  group(t, NULL);

  test_init(t);
  test_external_context(t);
  test_debug_mode(t);
  test_destroy(t);

  group(t, "end");
  test(t, "tests-ended", t->passed > 0);
  group(t, NULL);
}

int main(int argc, char **argv) {
  struct test_context context = { .passed = 0, .failed = 0, .group = NULL };

  printf("%sTesting pcomm...%s\n", COLOR_YELLOW, COLOR_OFF);

  run_tests(&context);

  if (context.failed == 0) {
    if (context.passed == 0) {
      printf("%sNo tests run.%s\n", COLOR_YELLOW, COLOR_OFF);
    } else {
      printf(
        "%sAll %d tests passed.%s\n",
        COLOR_GREEN,
        context.passed + context.failed,
        COLOR_OFF
      );
    }
  } else {
    printf(
      "%sFailed %d of %d tests.%s\n",
      COLOR_RED,
      context.failed,
      context.passed + context.failed,
      COLOR_OFF
    );
  }
}
