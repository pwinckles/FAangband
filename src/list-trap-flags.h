/* list-trap-flags.h - trap properties
 *
 * Adjusting these flags does not break savefiles. Flags below start from 1
 * on line 11, so a flag's sequence number is its line number minus 10.
 *
 *
 */

/*  symbol     descr */
TRF(NONE,        "")
TRF(RUNE,        "Is a rune")
TRF(TRAP,        "Is a player trap")
TRF(M_TRAP,      "Is a monster trap")
TRF(VISIBLE,     "Is visible")