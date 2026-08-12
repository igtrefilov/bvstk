/*
 * (c) 2026, SWD Embedded Systems Limited, http://www.kpda.ru
 */

/*
 * $QNXLicenseC:
 * Copyright 2007, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

#ifndef __PUBLIC_FPGA_H__
#define __PUBLIC_FPGA_H__

/*
 * Resource Manager Interface
 */

#define ZYNQ7000_FPGA_DEVICE "/dev/fpga"

/*
 * The following devctls are used by a client application
 * to control the FPGA interface.
 */

#include <devctl.h>
#include <stdint.h>

#define _DCMD_FPGA _DCMD_MISC

#define DCMD_FPGA_RESET                 __DION (_DCMD_FPGA, 1)
#define DCMD_FPGA_ENABLE_SEC            __DION (_DCMD_FPGA, 2)
#define DCMD_FPGA_DISABLE_SEC           __DION (_DCMD_FPGA, 3)
#define DCMD_FPGA_IS_PROG_DONE          __DIOF (_DCMD_FPGA, 4, uint8_t)

#endif /* __PUBLIC_FPGA_H__ */
