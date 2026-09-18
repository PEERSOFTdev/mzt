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

extern struct mnementry mnem1tbl[];
extern struct mnementry cb1tbl[];
extern struct mnementry reg1tbl[];
extern struct mnementry ddcb1tbl[];
extern struct mnementry dd11tbl[];
extern struct mnementry dd21tbl[];
extern struct mnementry ed1tbl[];
extern unsigned char opt1tbl[];
extern unsigned char ed1code[];
extern unsigned char dd1code[];
extern unsigned char cb1cycles[256];
extern unsigned char dd1cycles[256];
extern unsigned char ed1cycles[256];
extern unsigned char ed1cycles2[256];
extern unsigned char fd1cycles[256];
extern unsigned char ddcb1cycles[256];
extern unsigned char fdcb1cycles[256];

#endif	// _OPCODES_Z180_H
