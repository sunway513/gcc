/* Copyright (C) 2001, 2009 Free Software Foundation, Inc.
   This file was adapted from glibc sources.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

/* See an explanation about .init and .fini in crti.asm.  */

#ifdef __H8300H__
#ifdef __NORMAL_MODE__
	.h8300hn
#else
	.h8300h
#endif
#endif

#ifdef __H8300S__
#ifdef __NORMAL_MODE__
	.h8300sn
#else
	.h8300s
#endif
#endif
#ifdef __H8300SX__
#ifdef __NORMAL_MODE__
	.h8300sxn
#else
	.h8300sx
#endif
#endif
	.section .init, "ax", @progbits
	rts

	.section .fini, "ax", @progbits
	rts
