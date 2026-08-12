#pragma once

#include <furi.h>
#include <datetime/datetime.h>

#include "i2c_worker.h"

// Builds the human-readable report. The same text is what the screen shows
// and what lands on the SD card — a report you hand to a courier must not
// differ from the file you email afterwards.
//
// What the app managed to establish when nothing answered at all. NULL on the
// normal path. The person who most needs a document is the one whose chip
// never spoke -- they are the one being told to accept or refuse a parcel with
// no evidence either way -- and until this existed they were the only user who
// could not save one.
typedef struct {
    I2CBusCheck bus;
    bool pad_measured; // the user walked a wire onto a mode pad
    uint8_t pad_level; // I2CPadLevel
    const char* pad_labels; // the family of labels they said they were on
} SilentDiagnosis;

// disputed: the buyer has said this is not the part they ordered, which turns
// the document from an inspection note into a reason for refusing delivery.
void report_build(
    FuriString* out,
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const DateTime* dt,
    const SilentDiagnosis* silent);
