/*
 * (c) 2026, SWD Embedded Systems Limited, https://www.kpda.ru
 */

#ifndef __SYS_BITFIELD_H__
#define __SYS_BITFIELD_H__

#ifdef __ASSEMBLER__
#define __BITFIELD_CONST__(__c, __s) (__c)
#else
#define __BITFIELD_CONST__(__c, __s) (__c ## __s)
#endif

#ifdef __LP64__
#define __BITFIELD_CONST32__ U
#define __BITFIELD_CONST64__ UL
#else /*!__LP64__*/
#define __BITFIELD_CONST32__ U
#define __BITFIELD_CONST64__ ULL
#endif /*__LP64__*/

#define __BITFIELD_TYPE_HALF_MAX(__sf, __sz) (__BITFIELD_CONST__(1, __sf) << ((__sz) - 1))
#define __BITFIELD_TYPE_MAX(__sf, __sz) ((__BITFIELD_TYPE_HALF_MAX(__sf, __sz) - 1) + __BITFIELD_TYPE_HALF_MAX(__sf, __sz))

/*
 * Return a bitmask with bit __n set, where the least significant bit is bit 0.
 */
#define __BIT(__n, __sf, __sz) ((__BITFIELD_CONST__(1, __sf) << (__n)) & __BITFIELD_TYPE_MAX(__sf, __sz))
#define __BIT8(__n) __BIT(__n, __BITFIELD_CONST32__, 8)
#define __BIT16(__n) __BIT(__n, __BITFIELD_CONST32__, 16)
#define __BIT32(__n) __BIT(__n, __BITFIELD_CONST32__, 32)
#define __BIT64(__n) __BIT(__n, __BITFIELD_CONST64__, 64)

/*
 * Return a bitmask with bits __lo through __hi, inclusive, set.
 * The least significant bit is bit 0.
 */
#define __BITS(__hi, __lo, __sf, __sz) ((__BITFIELD_TYPE_MAX(__sf, __sz) << (__lo)) & (__BITFIELD_TYPE_MAX(__sf, __sz) >> ((__sz) - 1 - (__hi))))
#define __BITS8(__hi, __lo) __BITS(__hi, __lo, __BITFIELD_CONST32__, 8)
#define __BITS16(__hi, __lo) __BITS(__hi, __lo, __BITFIELD_CONST32__, 16)
#define __BITS32(__hi, __lo) __BITS(__hi, __lo, __BITFIELD_CONST32__, 32)
#define __BITS64(__hi, __lo) __BITS(__hi, __lo, __BITFIELD_CONST64__, 64)

/*
 * Return a bitmask with the first __n bits set.
 * That is, bits 0 through __n - 1, inclusive, set.
 */
#define __MASK(__n, __sf, __sz) __BITS((__n) - 1, 0, __sf, __sz)
#define __MASK8(__n) __BITS8((__n) - 1, 0)
#define __MASK16(__n) __BITS16((__n) - 1, 0)
#define __MASK32(__n) __BITS32((__n) - 1, 0)
#define __MASK64(__n) __BITS64((__n) - 1, 0)

/*
 * Find least significant bit that is set
 */
#define __BITFIELD_LOWEST_SET_BIT_VALUE(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))
/*
 * Left-shift bits __x into the bitfield defined by __mask, and return them.
 */
#define __SHIFTIN(__x, __mask) ((__x) * __BITFIELD_LOWEST_SET_BIT_VALUE(__mask))
/*
 * Extract and return the bitfield selected by __mask from __x,
 * right-shifting the bits so that the right most selected bit is at bit 0.
 */
#define __SHIFTOUT(__x, __mask) (((__x) & (__mask)) / __BITFIELD_LOWEST_SET_BIT_VALUE(__mask))
/*
 * Right-shift the bits in __mask so that the rightmost non-zero bit is at bit 0.
 * This is useful for finding the greatest unsigned value that a bit field can hold.
 * Note that __SHIFTOUT_MASK(m) = __SHIFTOUT(m, m).
 */
#define __SHIFTOUT_MASK(__mask) __SHIFTOUT((__mask), (__mask))

#endif /*__SYS_BITFIELD_H__*/
