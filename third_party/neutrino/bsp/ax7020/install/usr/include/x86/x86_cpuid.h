/*
 * (c) 2025, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * x86/x86_cpuid.h - x86 CPUID (CPU Identification)
 */

#ifndef __X86_CPUID_H__
#define __X86_CPUID_H__

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif

/*
 * CPUID feature bits (TODO: delete this)
 */
/* EDX register */
#define X86_FEATURE_FPU			_ONEBIT32L(0)
#define X86_FEATURE_VME			_ONEBIT32L(1)
#define X86_FEATURE_DE			_ONEBIT32L(2)
#define X86_FEATURE_PSE			_ONEBIT32L(3)
#define X86_FEATURE_TSC			_ONEBIT32L(4)
#define X86_FEATURE_MSR			_ONEBIT32L(5)
#define X86_FEATURE_PAE			_ONEBIT32L(6)
#define X86_FEATURE_MCE			_ONEBIT32L(7)
#define X86_FEATURE_CXS			_ONEBIT32L(8)
#define X86_FEATURE_APIC		_ONEBIT32L(9)
#define X86_FEATURE_SEP			_ONEBIT32L(11)
#define X86_FEATURE_MTRR		_ONEBIT32L(12)
#define X86_FEATURE_PGE			_ONEBIT32L(13)
#define X86_FEATURE_MCA			_ONEBIT32L(14)
#define X86_FEATURE_CMOV		_ONEBIT32L(15)
#define X86_FEATURE_PAT			_ONEBIT32L(16)
#define X86_FEATURE_PSE36		_ONEBIT32L(17)
#define X86_FEATURE_SN			_ONEBIT32L(18)
#define X86_FEATURE_CFLUSH		_ONEBIT32L(19)
#define X86_FEATURE_DS			_ONEBIT32L(21)
#define X86_FEATURE_ACPI		_ONEBIT32L(22)
#define X86_FEATURE_MMX			_ONEBIT32L(23)
#define X86_FEATURE_FXSR		_ONEBIT32L(24)
#define X86_FEATURE_SIMD		_ONEBIT32L(25)
#define X86_FEATURE_SSE2		_ONEBIT32L(26)
#define X86_FEATURE_SS			_ONEBIT32L(27)
#define X86_FEATURE_HTT			_ONEBIT32L(28)
#define X86_FEATURE_TM			_ONEBIT32L(29)
#define X86_FEATURE_PBE			_ONEBIT32L(31)

/* ECX register */
#define X86_FEATURE2_SSE3		_ONEBIT32L(0)
#define X86_FEATURE2_PCLMULQDQ	_ONEBIT32L(1)
#define X86_FEATURE2_DTES64		_ONEBIT32L(2)
#define X86_FEATURE2_MONITOR	_ONEBIT32L(3)
#define X86_FEATURE2_DS_CPL		_ONEBIT32L(4)
#define X86_FEATURE2_VMX		_ONEBIT32L(5)
#define X86_FEATURE2_SMX		_ONEBIT32L(6)
#define X86_FEATURE2_EIST		_ONEBIT32L(7)
#define X86_FEATURE2_TM2		_ONEBIT32L(8)
#define X86_FEATURE2_SSSE3		_ONEBIT32L(9)
#define X86_FEATURE2_CNXT_ID	_ONEBIT32L(10)
#define X86_FEATURE2_FMA		_ONEBIT32L(12)
#define X86_FEATURE2_CMPXCHG16B	_ONEBIT32L(13)
#define X86_FEATURE2_xTPR_UPDATE_CONTROL	_ONEBIT32L(14)
#define X86_FEATURE2_PDCM		_ONEBIT32L(15)
#define X86_FEATURE2_PCID		_ONEBIT32L(17)
#define X86_FEATURE2_DCA		_ONEBIT32L(18)
#define X86_FEATURE2_SSE4_1		_ONEBIT32L(19)
#define X86_FEATURE2_SSE4_2		_ONEBIT32L(20)
#define X86_FEATURE2_x2APIC		_ONEBIT32L(21)
#define X86_FEATURE2_MOVBEA		_ONEBIT32L(22)
#define X86_FEATURE2_POPCNT		_ONEBIT32L(23)
#define X86_FEATURE2_TSC_DEADLINE	_ONEBIT32L(24)
#define X86_FEATURE2_AESNI		_ONEBIT32L(25)
#define X86_FEATURE2_XSAVE		_ONEBIT32L(26)
#define X86_FEATURE2_OSXSAVE	_ONEBIT32L(27)
#define X86_FEATURE2_AVX		_ONEBIT32L(28)
#define X86_FEATURE2_F16C		_ONEBIT32L(29)
#define X86_FEATURE2_RDRAND		_ONEBIT32L(30)
#define X86_FEATURE2_HYPERVISOR	_ONEBIT32L(31)

// Vendor strings from CPUs
#define CPUID_VENDOR_AMD           "AuthenticAMD"
#define CPUID_VENDOR_INTEL         "GenuineIntel"
#define CPUID_VENDOR_VORTEX        "Vortex86 SoC"

// Vendor strings from hypervisors
#define CPUID_VENDOR_QEMU          "TCGTCGTCGTCG"
#define CPUID_VENDOR_KVM           "KVMKVMKVM\0\0\0"
#define CPUID_VENDOR_VMWARE        "VMwareVMware"
#define CPUID_VENDOR_VIRTUALBOX    "VBoxVBoxVBox"
#define CPUID_VENDOR_XEN           "XenVMMXenVMM"
#define CPUID_VENDOR_HYPERV        "Microsoft Hv"

typedef enum {
	CPUID_VENDORSTRING,
	CPUID_FEATURES,
	CPUID_POWER_MGMT = 0x6,
	CPUID_FREQUENCY = 0x15,

	CPUID_HYPERVISOR_VENDOR = 0x40000000,
	CPUID_HYPERVISOR_INTERFACE,
	CPUID_HYPERVISOR_FREQUENCY = 0x40000010,

	CPUID_EXTENDED_VENDORSTRING = 0x80000000,
	CPUID_EXTENDED_FEATURES,
} x86_cpuid_requests_t;

#ifndef	__ASSEMBLER__
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <x86/priv.h>

// CPUID Request: CPUID_FEATURES
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			stepping        : 4,
			model_base      : 4,
			family_base		: 4,
			type			: 2,
			reserved_0		: 2,
			model_extended	: 4,
			family_extended	: 8,
			reserved_1	    : 4;

	} __attribute__((__packed__)) bits;
} x86_cpuid_features_eax_t;

// CPUID Request: CPUID_FEATURES
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			brand_id        : 8,
			cpuflush      	: 8,
			cpu_count		: 8,
			apic_id			: 8;

	} __attribute__((__packed__)) bits;
} x86_cpuid_features_ebx_t;

// CPUID Request: CPUID_FEATURES
// ECX: CPU features [1/2]
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			sse3	: 1,
			pclmul	: 1,
			dtes64	: 1,
			mon		: 1,
			dscpl	: 1,
			vmx		: 1,
			smx		: 1,
			est		: 1,
			tm2		: 1,
			ssse3	: 1,
			cid		: 1,
			sdbg	: 1,
			fma		: 1,
			cx16	: 1,
			etprd	: 1,
			pdcm	: 1,
			__rsvd0	: 1,
			pcid	: 1,
			dca		: 1,
			sse4_1	: 1,
			sse4_2	: 1,
			x2apic	: 1,
			movbe	: 1,
			popcnt	: 1,
			tscd	: 1,
			aes		: 1,
			xsave	: 1,
			osxsave	: 1,
			avx		: 1,
			f16c	: 1,
			rdrand	: 1,
			hv		: 1;

	} __attribute__((__packed__)) bits;
} x86_cpuid_features_ecx_t;

// CPUID Request: CPUID_FEATURES
// EDX: CPU features [2/2]
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			fpu		: 1,
			vme		: 1,
			de		: 1,
			pse		: 1,
			tsc		: 1,
			msr		: 1,
			pae		: 1,
			mce		: 1,
			cx8		: 1,
			apic	: 1,
			__rsvd0 : 1,
			sep		: 1,
			mtrr	: 1,
			pge		: 1,
			mca		: 1,
			cmov	: 1,
			pat		: 1,
			pse36	: 1,
			psn		: 1,
			clfl	: 1,
			__rsvd1	: 1,
			dtes	: 1,
			acpi	: 1,
			mmx		: 1,
			fxsr	: 1,
			sse		: 1,
			sse2	: 1,
			ss		: 1,
			htt		: 1,
			tm1		: 1,
			ia64	: 1,
			pbe		: 1;

	} __attribute__((__packed__)) bits;
} x86_cpuid_features_edx_t;

// CPUID Request: CPUID_POWER_MGMT
// EAX: Power Management info
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			dts     : 1, // Digital thermal sensor
			ds 		: 1, // Dynamic acceleration
			arat	: 1, // Always running APIC timer
			__rsvd0	: 1,
			pln		: 1,
			ecmd	: 1,
			ptm		: 1,
			hwp	    : 1,
			hwp_not	: 1,
			hwp_act	: 1,
			hwp_epp	: 1,
			hwp_plr	: 1,
			__rsvd1	: 1,
			hdc		: 1,
			__rsvd2	: 19;

	} __attribute__((__packed__)) bits;
} x86_cpuid_power_mgmt_eax_t;

// CPUID Request: CPUID_EXTENDED_FEATURES
// ECX: CPU extended features [1/2]
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			ahf64	: 1,
			cmp		: 1,
			svm		: 1,
			eas		: 1,
			cr8d	: 1,
			lzcnt	: 1,
			sse4a	: 1,
			msse	: 1,
			d3nowp	: 1,
			osvw	: 1,
			ibs		: 1,
			xop		: 1,
			skinit	: 1,
			wdt		: 1,
			__rsvd0	: 1,
			lwp		: 1,
			fma4	: 1,
			tce		: 1,
			__rsvd1	: 1,
			nodeid	: 1,
			__rsvd2	: 1,
			tbm		: 1,
			topx	: 1,
			pcx_core: 1,
			pcx_nb	: 1,
			__rsvd3	: 1,
			dbx		: 1,
			perftsc	: 1,
			pcx_l2	: 1,
			monx	: 1,
			__rsvd4	: 1,
			__rsvd5	: 1;

	} __attribute__((__packed__)) bits;
} x86_cpuid_extended_features_ecx_t;

// CPUID Request: CPUID_EXTENDED_FEATURES
// EDX: CPU extended features [2/2]
typedef union {
	uint32_t raw;
	struct {
		uint32_t
			fpu		: 1,
			vme		: 1,
			de		: 1,
			pse		: 1,
			tsc		: 1,
			msr		: 1,
			pae		: 1,
			mce		: 1,
			cx8		: 1,
			apic	: 1,
			__rsvd0	: 1,
			sep		: 1,
			mtrr	: 1,
			pge		: 1,
			mca		: 1,
			cmov	: 1,
			pat		: 1,
			pse36	: 1,
			__rsvd1	: 1,
			mp		: 1,
			nx		: 1,
			__rsvd2	: 1,
			mmxp	: 1,
			mmx		: 1,
			fxsr	: 1,
			ffxsr	: 1,
			pg1g	: 1,
			tscp	: 1,
			__rsvd3	: 1,
			lm		: 1,
			d3nowpp	: 1,
			d3now	: 1;

	} __attribute__((__packed__)) bits;
} x86_cpuid_extended_features_edx_t;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * CPUID request
 */
static inline const int
x86_get_cpuid(uint32_t level, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#define CPUID_LEVEL_MASK	0xC0000000
	uint32_t __max_level, __ebx, __ecx, __edx;
	cpuid(level & CPUID_LEVEL_MASK, 0, &__max_level, &__ebx, &__ecx, &__edx);

	if ((__max_level == 0) || (level > __max_level)) return 0;

	cpuid(level, 0, eax, ebx, ecx ,edx);
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU Vendor ID string.
 *
 * @param vendor_id	  Pointer vendor id string.
 * @return Max supported CPUID request value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_vendor_id(char *vendor_id) {
	uint32_t eax, ebx, ecx, edx;
	char vendor_id_string[12];

	int ret = x86_get_cpuid(CPUID_VENDORSTRING, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}

    *(uint32_t*)&vendor_id_string[0] = ebx;
    *(uint32_t*)&vendor_id_string[4] = edx;
    *(uint32_t*)&vendor_id_string[8] = ecx;

	strcpy(vendor_id, vendor_id_string);

    return (eax); // return highest supported CPUID function
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU Signature.
 *
 * @return CPU Signature value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_cpu_signature(void) {
    x86_cpuid_features_eax_t cpu_signature;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    cpu_signature.raw = eax;
    return (cpu_signature.raw);
}

/**
 * Get CPU Family.
 *
 * @return CPU Family value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint16_t
x86_cpuid_get_cpu_family(void) {
    x86_cpuid_features_eax_t cpu_signature;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    cpu_signature.raw = eax;
    return ((uint16_t)cpu_signature.bits.family_extended + cpu_signature.bits.family_base);
}

/**
 * Get CPU Model.
 *
 * @return CPU Model value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint16_t
x86_cpuid_get_cpu_model(void) {
    x86_cpuid_features_eax_t cpu_signature;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    cpu_signature.raw = eax;
    return ((uint16_t)(cpu_signature.bits.model_extended << 4) + cpu_signature.bits.model_base);
}

/**
 * Get CPU Stepping.
 *
 * @return CPU Stepping value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint8_t
x86_cpuid_get_cpu_stepping(void) {
    x86_cpuid_features_eax_t cpu_signature;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    cpu_signature.raw = eax;
    return ((uint8_t)cpu_signature.bits.stepping);
}

/**
 * Get CPU Type.
 *
 * @return CPU Type value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint8_t
x86_cpuid_get_cpu_type(void) {
    x86_cpuid_features_eax_t cpu_signature;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    cpu_signature.raw = eax;
    return ((uint8_t)cpu_signature.bits.type);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU APIC ID number.
 *
 * @return CPU APIC ID value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint8_t
x86_cpuid_get_apic_id(void) {
	x86_cpuid_features_ebx_t cpu_id;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
	cpu_id.raw = ebx;
	return ((uint8_t)cpu_id.bits.apic_id);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Is CPU feature XSAVE supported.
 *
 * @retval True - XSAVE is supported.
 * @retval False - XSAVE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_xsave(void) {
	x86_cpuid_features_ecx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = ecx;
	if (features.bits.xsave) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature AVX supported.
 *
 * @retval True - AVX is supported.
 * @retval False - AVX is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_avx(void) {
	x86_cpuid_features_ecx_t features;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = ecx;
	if (features.bits.avx) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature HV supported.
 *
 * @retval True - HV is supported.
 * @retval False - HV is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_hv(void) {
	x86_cpuid_features_ecx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = ecx;
	if (features.bits.hv) {
		return (true);
	}
	return (false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Is CPU feature FPU supported.
 *
 * @retval True - FPU is supported.
 * @retval False - FPU is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_fpu(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.fpu) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature PSE supported.
 *
 * @retval True - PSE is supported.
 * @retval False - PSE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_pse(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.pse) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature TSC supported.
 *
 * @retval True - TSC is supported.
 * @retval False - TSC is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_tsc(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.tsc) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature PAE supported.
 *
 * @retval True - PAE is supported.
 * @retval False - PAE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_pae(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.pae) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature MCE supported.
 *
 * @retval True - MCE is supported.
 * @retval False - MCE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_mce(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.mce) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature SEP supported.
 *
 * @retval True - SEP is supported.
 * @retval False - SEP is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_sep(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.sep) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature MTRR supported.
 *
 * @retval True - MTRR is supported.
 * @retval False - MTRR is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_mtrr(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.mtrr) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature PGE supported.
 *
 * @retval True - PGE is supported.
 * @retval False - PGE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_pge(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.pge) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature CMOV supported.
 *
 * @retval True - CMOV is supported.
 * @retval False - CMOV is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_cmov(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.cmov) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature PAT supported.
 *
 * @retval True - PAT is supported.
 * @retval False - PAT is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_pat(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.pat) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature MMX supported.
 *
 * @retval True - MMX is supported.
 * @retval False - MMX is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_mmx(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.mmx) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature FXSR supported.
 *
 * @retval True - FXSR is supported.
 * @retval False - FXSR is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_fxsr(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.fxsr) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature SSE supported.
 *
 * @retval True - SSE is supported.
 * @retval False - SSE is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_sse(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.sse) {
		return (true);
	}
	return (false);
}

/**
 * Is CPU feature SSE2 supported.
 *
 * @retval True - SSE2 is supported.
 * @retval False - SSE2 is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_sse2(void) {
	x86_cpuid_features_edx_t features;
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features.raw = edx;
	if (features.bits.sse2) {
		return (true);
	}
	return (false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU core crystal clock frequency in Hz.
 *
 * @return CPU core crystal clock frequency in Hz.
 * @retval 0: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_core_clock_frequency(void) {
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_FREQUENCY, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return 0;
	}
	return (ecx);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Is APIC timer present.
 *
 * @retval True - APIC timer is present.
 * @retval False - APIC timer is not present.
 */
static inline const bool
x86_cpuid_is_apic_timer_running(void) {
	x86_cpuid_power_mgmt_eax_t power_mgmt;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_POWER_MGMT, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	power_mgmt.raw = eax;
	if (power_mgmt.bits.arat) {
		return (true);
	}
	return (false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU Hypervisor Vendor ID string.
 *
 * @param vendor_id	  Pointer vendor id string.
 * @return Max supported HV CPUID request value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_hv_vendor_id(char *vendor_id) {
	uint32_t eax, ebx, ecx, edx;
	char vendor_id_string[12];

	int ret = x86_get_cpuid(CPUID_HYPERVISOR_VENDOR, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}

    *(uint32_t*)&vendor_id_string[0] = ebx;
    *(uint32_t*)&vendor_id_string[4] = ecx;
    *(uint32_t*)&vendor_id_string[8] = edx;

	strcpy(vendor_id, vendor_id_string);

    return (eax); // return highest supported CPUID function
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU Hypervisor interface signature.
 *
 * @return CPU Hypervisor interface signature.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_hv_signature(void) {
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_HYPERVISOR_INTERFACE, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    return (eax);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get CPU Hypervisor LAPIC frequency.
 *
 * @return CPU Hypervisor LAPIC frequency value.
 * @retval -1: error - unsupported CPUID request.
 */
static inline const uint32_t
x86_cpuid_get_hv_lapic_frequency(void) {
	uint32_t eax, ebx, ecx, edx;

	int ret = x86_get_cpuid(CPUID_HYPERVISOR_FREQUENCY, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return -1;
	}
    return (ebx);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Is CPU feature NX supported.
 *
 * @retval True - NX is supported.
 * @retval False - NX is not supported.
 */
static inline const bool
x86_cpuid_is_support_feature_nx(void) {
	x86_cpuid_extended_features_edx_t features_extended;
	uint32_t eax, ebx, ecx, edx;

    int ret = x86_get_cpuid(CPUID_EXTENDED_FEATURES, &eax, &ebx, &ecx, &edx);
	if(ret != 1) {
		return (false);
	}
	features_extended.raw = edx;
	if (features_extended.bits.nx) {
		return (true);
	}
	return (false);
}
#endif /* __ASSEMBLER__ */
#endif /* __X86_CPUID_H__ */
