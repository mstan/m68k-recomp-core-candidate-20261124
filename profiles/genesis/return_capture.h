/*
 * return_capture.h — conservative proof for 68000 return-address consumers.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rom_parser.h"

/*
 * True only when every reachable path from `start` consumes exactly one longword from
 * (A7)+ and reaches RTS without another stack operation or call. Both sides
 * of conditional branches are checked before and after the capture. Frame
 * reconstruction, unknown exits, cycles, malformed instructions and exhaustion
 * of the 96-node (PC, capture-state) budget conservatively return false.
 *
 * This covers both the straight-line Obj_WaitOffscreen idiom and helpers such
 * as Puyo Puyo's timer routine, whose conditional paths all consume the
 * caller's return address at different instructions. A false result means
 * unproven; the caller must retain ordinary/dynamic return handling.
 */
bool m68k_return_capture_is_unconditional(const GenesisRom *rom, uint32_t start);

/* Every path must be understood by the same bounded proof. Plain RTS exits
 * may coexist with exits consuming complete return slots through longword,
 * word or MOVEM pops. The path-local byte count selects the actual unwind;
 * this does not authorize unconditional call-site tail transfer. */
bool m68k_return_capture_has_capturing_exit(const GenesisRom *rom, uint32_t start);
