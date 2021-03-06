/***********************************************************************
 *
 * File: stgfb/Gamma/sti7200/sti7200cut2tvoutreg.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200CUT2TVOUTREG_H
#define _STI7200CUT2TVOUTREG_H

/*
 * Basically we have the same Local TVout/HDFormatter as the 7111
 */ 
#include <Gamma/sti7111/sti7111tvoutreg.h>

/*
 * Clock control for SCART output from SDTVout on HD DACs
 */
#define TVOUT_GP_OUT_HD_PIX_SD_N_HD (1L<<0)

#endif // _STI7200CUT2TVOUTREG_H
