#ifndef RICH4_SDL_MKF_TEST_H
#define RICH4_SDL_MKF_TEST_H

/* Test-only hooks; declarations and definitions exist only with
 * RICH4_MKF_TESTING. A value of zero fails the next operation, while a
 * positive value allows that many operations before one failure. */
#ifdef RICH4_MKF_TESTING
void rich4_mkf_test_fail_alloc_after(int successful_operations);
void rich4_mkf_test_fail_read_after(int successful_operations);
void rich4_mkf_test_fail_seek_after(int successful_operations);
void rich4_mkf_test_clear_failures(void);
#endif

#endif

