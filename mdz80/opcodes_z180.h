/***************************************************************************
 *   Copyright (C) 2021 by PEERSOFT   *
 *   peersoft@outlook.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/*
 * Z80 Disassembler
 * Copyright (C) 1990-2007 by Jeffery L. Post
 * j_post <AT> pacbell <DOT> net
 *
 * dz80table.h - Z80 disassembler tables
 *
 * Version 3.4.1 - 2007/09/02
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef	_OPCODES_Z180_H
#define	_OPCODES_Z180_H

// The register table, CB table, indexed (DD/FD) opcodes and plain-opcode
// instruction sizes are identical between Z80 and the HD64180, so only the
// ED-prefix decode flags and the T-state tables need HD64180-specific
// versions here. The ED-prefix mnemonics themselves live in the shared,
// mode-independent edtbl in opcodes_z80.c (see ed1code's comment).

extern unsigned char ed1code[];
extern unsigned char cycles1[256];
extern unsigned char cycles21[256];
extern unsigned char cb1cycles[256];
extern unsigned char dd1cycles[256];
extern unsigned char ed1cycles[256];
extern unsigned char ed1cycles2[256];
extern unsigned char fd1cycles[256];
extern unsigned char ddcb1cycles[256];
extern unsigned char fdcb1cycles[256];

#endif	// _OPCODES_Z180_H
