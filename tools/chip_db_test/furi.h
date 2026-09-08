// Just enough <furi.h> for chip_db.c to build on a host compiler. chip_db.c is
// the one file in the app with no hardware in it beyond three I2C reads, which
// is what makes it testable here at all -- and it is also the file where a
// wrong branch turns a genuine part into an accusation, so it is the one worth
// the stub.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The real one sleeps. Here it is a no-op: nothing in the test has a bus that
// needs settling, and a test that takes real milliseconds per retry is a test
// people stop running.
static inline void furi_delay_ms(uint32_t ms) {
    (void)ms;
}
