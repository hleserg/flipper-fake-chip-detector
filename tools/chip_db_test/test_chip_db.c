// What chip_db_identify says about a bus that is not there.
//
// Every case below is one that shipped wrong at least once: a genuine DS3231
// named as one of the two parts that share its address, a part that ACKs and
// then answers nothing coming back with an empty detail screen, and an address
// nobody has ever heard of taking a branch that could not be reached. They are
// cheap to check without a Flipper because chip_db.c talks to the bus through
// three functions and nothing else -- so this file is those three functions.
//
//   cc -I fake_chip_detector -I tools/chip_db_test fake_chip_detector/chip_db.c
//      tools/chip_db_test/test_chip_db.c -o /tmp/t && /tmp/t

#include "chip_db.h"
#include "i2c_worker.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---------------- The bus that is not there ---------------- */

typedef struct {
    uint16_t reg;
    uint8_t value;
} RegVal;

static struct {
    uint8_t addr; // the one address that ACKs
    const RegVal* regs;
    size_t reg_count;
    bool reads_fail; // ACKs its address, NAKs every register read
    bool has_default; // registers outside the table read back as `fill`
    uint8_t fill;
} bus;

static bool bus_byte(uint8_t addr7, uint16_t reg, uint8_t* out) {
    if(addr7 != bus.addr || bus.reads_fail) return false;
    for(size_t i = 0; i < bus.reg_count; i++) {
        if(bus.regs[i].reg == reg) {
            *out = bus.regs[i].value;
            return true;
        }
    }
    if(!bus.has_default) return false;
    *out = bus.fill;
    return true;
}

bool i2c_worker_read_reg(uint8_t addr7, uint8_t reg, uint8_t* value, uint32_t timeout_ms) {
    (void)timeout_ms;
    return bus_byte(addr7, reg, value);
}

bool i2c_worker_read_mem(
    uint8_t addr7,
    uint8_t reg,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms) {
    (void)timeout_ms;
    for(size_t i = 0; i < len; i++) {
        if(!bus_byte(addr7, (uint16_t)(reg + i), &data[i])) return false;
    }
    return true;
}

bool i2c_worker_read_reg16_addr(
    uint8_t addr7,
    uint16_t reg,
    uint8_t* data,
    size_t len,
    uint32_t timeout_ms) {
    (void)timeout_ms;
    for(size_t i = 0; i < len; i++) {
        if(!bus_byte(addr7, (uint16_t)(reg + i), &data[i])) return false;
    }
    return true;
}

/* ---------------- Reading the database back ---------------- */

static bool entry_has_addr(const ChipEntry* chip, uint8_t addr7) {
    if(chip->range_lo && addr7 >= chip->range_lo && addr7 <= chip->range_hi) return true;
    for(size_t i = 0; i < CHIP_MAX_ADDRS && chip->addrs[i] != 0xFF; i++) {
        if(chip->addrs[i] == addr7) return true;
    }
    return false;
}

// The addresses are picked out of the database rather than written down here,
// so that a row added later moves the test instead of breaking it.
static uint8_t addr_with_no_id_count(size_t want) {
    for(uint8_t a = 0x08; a <= 0x77; a++) {
        if(chip_db_no_id_at(a, NULL, 0) == want) return a;
    }
    return 0;
}

// An address where something has ID registers to try. A silent device is only
// silent about a question that was asked, and at an address whose only parts
// have no ID register there is no question to ask.
static uint8_t addr_with_id_checks(void) {
    for(uint8_t a = 0x08; a <= 0x77; a++) {
        for(size_t i = 0; i < chip_db_count(); i++) {
            const ChipEntry* chip = chip_db_get(i);
            if(chip->checks && entry_has_addr(chip, a)) return a;
        }
    }
    return 0;
}

static uint8_t unknown_addr(void) {
    for(uint8_t a = 0x08; a <= 0x77; a++) {
        bool claimed = false;
        for(size_t i = 0; i < chip_db_count() && !claimed; i++) {
            claimed = entry_has_addr(chip_db_get(i), a);
        }
        if(!claimed) return a;
    }
    return 0;
}

// A part with more than one ID register, so that "some of them are right" is a
// state it can be in at all.
static const ChipEntry* entry_with_several_checks(void) {
    for(size_t i = 0; i < chip_db_count(); i++) {
        const ChipEntry* chip = chip_db_get(i);
        if(chip->checks && chip->check_count >= 2 && chip->addrs[0] != 0xFF) return chip;
    }
    return NULL;
}

/* ---------------- The cases ---------------- */

static void case_two_no_id_parts_stay_unnamed(void) {
    // A real DS3231 at 0x68. Its registers read back as seconds and minutes,
    // which match no ID check anywhere -- and a DS1307 at the same address
    // would read back exactly the same way, because neither part has an ID
    // register to tell them apart with. Naming the first of the two was what
    // this app used to do.
    const uint8_t addr = addr_with_no_id_count(2);
    assert(addr && "no address in the database has exactly two ID-less parts");

    bus = (typeof(bus)){.addr = addr, .has_default = true, .fill = 0x00};
    ChipIdentification id;
    chip_db_identify(addr, &id);

    assert(id.verdict == VerdictAmbiguous);
    assert(id.chip == NULL);
    assert(id.candidates == 2);
    assert(chip_verdict_is_good(id.verdict)); // ambiguous is not an accusation

    const ChipEntry* named[4] = {0};
    assert(chip_db_no_id_at(addr, named, 4) == 2);
    assert(named[0] && named[1] && named[0] != named[1]);
    printf("  0x%02X: %s / %s -> AMBIGUOUS\n", addr, named[0]->name, named[1]->name);
}

static void case_one_no_id_part_is_still_named(void) {
    const uint8_t addr = addr_with_no_id_count(1);
    assert(addr && "no address in the database has exactly one ID-less part");

    bus = (typeof(bus)){.addr = addr, .has_default = true, .fill = 0x00};
    ChipIdentification id;
    chip_db_identify(addr, &id);

    assert(id.verdict == VerdictDetectedNoId);
    assert(id.chip != NULL);
    printf("  0x%02X: %s -> DETECTED (no ID reg)\n", addr, id.chip->name);
}

static void case_silent_device_keeps_its_reads(void) {
    // ACKs the address, NAKs every register. The verdict was already right;
    // what was missing was the evidence -- read_count came back zero and the
    // detail screen had nothing on it, which is the one screen that could have
    // shown which registers were tried.
    const uint8_t addr = addr_with_id_checks();
    assert(addr && "no address in the database has a chip with ID registers");
    bus = (typeof(bus)){.addr = addr, .reads_fail = true};

    ChipIdentification id;
    chip_db_identify(addr, &id);

    assert(id.verdict == VerdictNoAnswer);
    assert(id.chip == NULL); // a part that will not read has identified nothing
    assert(id.read_count > 0);
    for(uint8_t r = 0; r < id.read_count; r++) {
        assert(!id.reads[r].read_ok);
    }
    printf("  0x%02X: silent -> NO ANSWER, %u reads shown\n", addr, id.read_count);
}

static void case_unknown_address_is_probed(void) {
    // This branch was unreachable: the no-answer test above it fired first on
    // an address with no candidates, because "no read has been attempted" and
    // "every read failed" were the same flag.
    const uint8_t addr = unknown_addr();
    assert(addr && "every address 0x08-0x77 is claimed by the database");

    bus = (typeof(bus)){.addr = addr, .has_default = true, .fill = 0x5A};
    ChipIdentification id;
    chip_db_identify(addr, &id);

    assert(id.verdict == VerdictUnknown);
    assert(id.read_count == CHIP_MAX_CHECKS);
    for(uint8_t r = 0; r < id.read_count; r++) {
        assert(id.reads[r].read_ok && !id.reads[r].has_expected);
        assert(id.reads[r].actual == 0x5A);
    }
    printf("  0x%02X: unknown, answers -> UNKNOWN with %u raw bytes\n", addr, id.read_count);

    // Same address, nothing readable. "Not in the database" and "would not
    // talk" are different faults and only one of them is about the database.
    bus.has_default = false;
    bus.reads_fail = true;
    chip_db_identify(addr, &id);
    assert(id.verdict == VerdictNoAnswer);
    assert(id.read_count == CHIP_MAX_CHECKS);
    printf("  0x%02X: unknown, silent -> NO ANSWER\n", addr);
}

static void case_partial_match_still_accuses(void) {
    // The counterfeit case the app exists for, and the one thing none of the
    // changes above may soften: some of a known part's IDs right, the rest
    // wrong, is still evidence.
    const ChipEntry* chip = entry_with_several_checks();
    assert(chip && "no chip in the database has two ID registers");

    RegVal regs[CHIP_MAX_CHECKS];
    for(uint8_t i = 0; i < chip->check_count; i++) {
        regs[i].reg = chip->checks[i].reg;
        regs[i].value = (uint8_t)(i == 0 ? chip->checks[i].expected : ~chip->checks[i].expected);
    }
    bus = (typeof(bus)){
        .addr = chip->addrs[0], .regs = regs, .reg_count = chip->check_count};

    ChipIdentification id;
    chip_db_identify(chip->addrs[0], &id);
    assert(id.verdict == VerdictWrongChip);
    assert(id.chip == chip);
    assert(!chip_verdict_is_good(id.verdict));
    printf("  0x%02X: half a %s -> LIKELY FAKE\n", chip->addrs[0], chip->name);
}

int main(void) {
    printf("chip_db_identify:\n");
    case_two_no_id_parts_stay_unnamed();
    case_one_no_id_part_is_still_named();
    case_silent_device_keeps_its_reads();
    case_unknown_address_is_probed();
    case_partial_match_still_accuses();
    printf("ok\n");
    return 0;
}
