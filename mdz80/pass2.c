/***************************************************************************
 *   Copyright (C) 2008 by Piergiorgio Betti   *
 *   pbetti@lpconsul.net   *
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
 *   Modified 2026 by PEERSOFT <97554883+PEERSOFTdev@users.noreply.github.com>:
 *   Cpu-type wiring, ED-prefix decode fixes, and HD64180/Z180
 *   operand handling.
 */


/*
 * Z80 Disassembler
 * Copyright (C) 1990-2007 by Jeffery L. Post
 * j_post <AT> pacbell <DOT> net
 *
 * dz80pass2.c - Disassembly pass 2
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "mdz80.h"
#include "pass1.h"
#include "pass2.h"
#include "opcodes_z80.h"
#include "opcodes_z180.h"
#include "opcodes_8080.h"

int opcount;				// bytes/opcode count

//
// Pass two of disassembly.
//
// Rescan data array and generate output file. Generate label if location
// flagged as referenced by some other opcode. If un-initialized data is
// encountered, ignore it but set skip flag so that an ORG statement can
// be generated when the next initialized data is found.
//

void pass2( void )
{
	char c, * cptr;
	int skip, year;
	int i, l, q, temp;
	byte j, k, k2;
	int pflag;
	time_t tp;
	int cyc, cyc2;		// for cycle counting
	unsigned char *edc = ( cputype == c_64180 ) ? ed1code : edcode;

	// T-state tables: pick the HD64180 set when disassembling for that CPU,
	// otherwise the standard Z80 set (8080 timing is not yet implemented,
	// same as before this table was split out - see cycleflag block below)
	unsigned char *cyctbl    = ( cputype == c_64180 ) ? cycles1     : cycles;
	unsigned char *cyc2tbl   = ( cputype == c_64180 ) ? cycles21    : cycles2;
	unsigned char *edcyctbl  = ( cputype == c_64180 ) ? ed1cycles   : edcycles;
	unsigned char *edcyc2tbl = ( cputype == c_64180 ) ? ed1cycles2  : edcycles2;
	unsigned char *cbcyctbl  = ( cputype == c_64180 ) ? cb1cycles   : cbcycles;
	unsigned char *ddcyctbl  = ( cputype == c_64180 ) ? dd1cycles   : ddcycles;
	unsigned char *fdcyctbl  = ( cputype == c_64180 ) ? fd1cycles   : fdcycles;
	unsigned char *ddcbcyctbl = ( cputype == c_64180 ) ? ddcb1cycles : ddcbcycles;
	unsigned char *fdcbcyctbl = ( cputype == c_64180 ) ? fdcb1cycles : fdcbcycles;

	k = 0;
	j = 0xff;
	skip = TRUE;
	dump = FALSE;
	byte_cnt = 0;
	asc_cnt = 0;
	newline = FALSE;

	printf( "\nPass 2 0000" );

	if ( ( fp = fopen( dst, "w" ) ) == NULL ) {	// open output source file
		printf( "\n* Can't open file %s *\n", dst );
		exit( FILE_ERROR );
	}

	fprintf( fp, ";\n;  MDZ80 V%d.%d.%d ", DVERSION, MAJORREV, MINORREV );	// output header

	switch ( cputype ) {
		case c_8080:
			fprintf( fp, "8080" );
			break;

		case c_64180:
			fprintf( fp, "64180" );
			break;

		default:
			fprintf( fp, "Z80" );
			break;
	}

	fprintf( fp, " Disassembly of %s", src );

	time( &tp );					 // get current time
	date_time = localtime( &tp );			// convert to hr/min/day etc
	year = date_time->tm_year + 1900;

	fprintf( fp, "\n;  %d/%02d/%02d %02d:%02d", year,
	         date_time->tm_mon + 1,
	         date_time->tm_mday,
	         date_time->tm_hour,
	         date_time->tm_min );

//
//  Generate output file
//
	for ( i = offset; i < himark; ) {
		if ( pgmflags[i] & PF_CMT ) {
			if ( byte_cnt )			// dump any pending binary
				dump_bytes( i );

			if ( asc_cnt )			// dump any accumulated ascii
				dump_ascii( i );

			output_comment( i );
		}

		if ( pgmflags[i] & PF_PATCH ) {
			if ( byte_cnt )			// dump any pending binary
				dump_bytes( i );

			if ( asc_cnt )			// dump any accumulated ascii
				dump_ascii( i );

			output_patch( i );
		}

		l = i & 0xff;
		pflag = pgmflags[i];

		if ( pflag & PF_NOINIT ) {		// ignore un-initialized data
			if ( byte_cnt )			// if binary or ascii data in buffers...
				dump_bytes( i );	// output them now

			if ( asc_cnt )
				dump_ascii( i );

			if ( dump )			// if we had to flush buffers...
			{				// do a newline, since we will
				dump = FALSE;		// be doing an org (or end)
				fprintf( fp, "\n;" );
				newline = TRUE;
			}

			i++;				// next program location
			skip = TRUE;			// flag skip for ORG statement
			j = -1;
		} else if ( pflag & ( PF_ADRS | PF_WORD | PF_BYTE | PF_ASCII ) ) {											// if not executable code
			if ( !( j & 0x80 ) && !skip ) {
				j = -1;
				fprintf( fp, "\n;" );
				newline = TRUE;
			}
			// ignore irrelevant bits
			switch ( pflag & ( PF_ADRS | PF_WORD | PF_BYTE | PF_ASCII ) ) {
			case PF_ASCII:						// if ascii text...
				if ( byte_cnt )					// dump any pending binary
					dump_bytes( i );

				if ( skip ) {					// insert ORG statement
					if ( !newline )
						fprintf( fp, "\n;" );

					fprintf( fp, "\n\t%s\t", orgstr );
					puthex( i );
					fprintf( fp, "\n;" );
					newline = TRUE;
					skip = FALSE;				// reset skip flag
				}
				// if it really is ascii...
				if ( is_ascii( pgmmem[i] ) ) {
					string[asc_cnt] = pgmmem[i];
					asc_cnt++;

					if ( asc_cnt >= ASCLIMIT )
						dump_ascii( i + 1 );
				} else {					// else treat it as binary data
					pgmflags[i] |= PF_BYTE;

					if ( asc_cnt )				// dump any accumulated ascii
						dump_ascii( i );

					byte_data[byte_cnt] = pgmmem[i];	// add data to buffer
					byte_cnt++;
				}
				break;

			case PF_WORD:						// if word data...
				if ( byte_cnt )					// dump any binary or ascii in buffers
					dump_bytes( i );

				if ( asc_cnt )
					dump_ascii( i );

				if ( skip ) {					// insert ORG statement
					if ( !newline )
						fprintf( fp, "\n;" );

					fprintf( fp, "\n\t%s\t", orgstr );
					puthex( i );
					fprintf( fp, "\n;" );
					newline = TRUE;
					skip = FALSE;				// reset skip flag
				}

				chk_label( i );					// add label if referenced
				q = ( ( int ) pgmmem[i + 1] ) << 8;		// get word value
				temp = pgmmem[i] & 0xff;
				q |= temp;
				fprintf( fp, "%s\t", defwstr );

				if ( pgmflags[i] & PF_LABEL )
					cptr = find_entry( q, label_count, lab_val_index );	// see if label exists
				else
					cptr = find_entry( q, symbol_count, sym_val_index );	// see if symbol exists

				if ( cptr == NULL )						// if not, do hex value
					puthex( q );
				else
					fprintf( fp, "%s", cptr );				// else output symbol

				if ( hexflag ) {						// add comment field
					fprintf( fp, "\t\t; %04X   %02X %02X      %c%c",
					         i, pgmmem[i], pgmmem[i + 1],
					         ascii( pgmmem[i] ), ascii( pgmmem[i + 1] ) );

				}

				if ( pgmflags[i] & PF_ICMT )
					output_icomment( i );

				i++;

				if ( pgmflags[i + 2] != PF_ADRS && pgmflags[i + 2] != PF_WORD ) {
					fprintf( fp, "\n;" );
					newline = TRUE;
				}
				break;

			case PF_ADRS:						// if address data...
				if ( byte_cnt )					// output any pending binary or
					dump_bytes( i );			// ascii data from buffers

				if ( asc_cnt )
					dump_ascii( i );

				if ( skip ) {					// insert ORG statement
					if ( !newline )
						fprintf( fp, "\n;" );

					fprintf( fp, "\n\t%s\t", orgstr );
					puthex( i );
					fprintf( fp, "\n;" );
					newline = TRUE;
					skip = FALSE;				// reset skip flag
				}

				chk_label( i );					// add label if referenced
				q = ( ( int ) pgmmem[i + 1] ) << 8;		// get address value
				temp = pgmmem[i] & 0xff;
				q |= temp;
				fprintf( fp, "%s\t", defwstr );
				cptr = find_entry( q, label_count, lab_val_index );	// see if label exists

				if ( cptr == NULL ) {					// if not, output hex
					if ( !pgmflags[i] & PF_LABEL )			// search symbol table only if not defined as label
						cptr = find_entry( q, symbol_count, sym_val_index );

					if ( cptr == NULL ) {
						if ( !upperflag )
							fprintf( fp, "X%04x", q );
						else
							fprintf( fp, "X%04X", q );
					} else
						fprintf( fp, "%s", cptr );	// output symbol text
				} else
					fprintf( fp, "%s", cptr );		// else output label text

				if ( hexflag ) {				// do comment field
					fprintf( fp, "\t\t; %04X   %02X %02X      %c%c",
					         i, pgmmem[i], pgmmem[i + 1],
					         ascii( pgmmem[i] ), ascii( pgmmem[i + 1] ) );
				}

				if ( pgmflags[i] & PF_ICMT )
					output_icomment( i );

				i++;

				if ( pgmflags[i + 2] != PF_ADRS ) {
					fprintf( fp, "\n;" );
					newline = TRUE;
				}
				break;

			default:						// default = binary data...
				if ( asc_cnt )					// output any pending ascii data
					dump_ascii( i );

				if ( skip ) {					// insert ORG statement
					if ( !newline )
						fprintf( fp, "\n;" );

					fprintf( fp, "\n\t%s\t", orgstr );
					puthex( i );
					fprintf( fp, "\n;" );
					newline = TRUE;
					skip = FALSE;				// reset skip flag
				}

				byte_data[byte_cnt] = pgmmem[i];		// add data to buffer
				byte_cnt++;

				if ( byte_cnt >= BYTELIMIT )			// if end of buffer...
					dump_bytes( i + 1 );			// dump accumulated data
			}

			i++;							// next program location

			if ( !( pgmflags[i] & PF_NOINIT ) && ( pgmflags[i] & PF_ASCII ) ) {		// if next byte is flagged as
				if ( !is_ascii( pgmmem[i] ) )			// ascii, but is not...
					pgmflags[i] |= PF_BYTE;			// then flag it as binary
			}
		}

//
// If previous data was an unconditional transfer, AND current
// location is not referenced by some other opcode, AND current
// byte is 0 or 0FFH, then treat this byte as un-initialized data.
//
		else if ( ( j & 0x10 ) && ( !( pflag & PF_REF ) ) &&
		                ( ( pgmmem[i] == 0 ) || ( pgmmem[i] == NO_DATA ) ) &&
		                ( !( pflag & PF_FORCE ) ) ) {
			if ( byte_cnt )						// since we're going to skip some
				dump_bytes( i );				// data, output any pending ascii

			if ( asc_cnt )						// or binary data remaining in buffers
				dump_ascii( i );

			if ( dump )						// if ascii or binary output was done,
			{							// stick in a newline
				fprintf( fp, "\n;" );
				dump = FALSE;
				newline = TRUE;
			}

			pgmflags[i] = PF_NOINIT;				// flag as uninitialized data
			i++;
			skip = TRUE;
			byte_cnt = 0;
		}

//
// If previous opcode was 0 or 0ffH, AND current location is not
// referenced but is initialized, AND current opcode is 0 or 0ffH,
// un-initialize it.
//
		else if ( ( ( k == 0 ) || ( k == 0xff ) ) &&
		                ( !( pflag & PF_REF ) ) &&
		                ( ( pgmmem[i] == 0 ) || ( pgmmem[i] == NO_DATA ) ) &&
		                !( pflag & PF_NOINIT ) &&
		                !( pflag & PF_FORCE ) ) {
			pgmflags[i] = PF_NOINIT;
			i++;
			skip = TRUE;
			j = -1;
		}

		else {								// IT'S EXECUTABLE CODE!
			pgmflags[i] &= ~PF_NOINIT;				// clear for label search in pass 3

			if ( byte_cnt )						// if any ascii or binary data remains
				dump_bytes( i );				// in the buffers, output them now

			if ( asc_cnt )
				dump_ascii( i );

			if ( dump ) {
				fprintf( fp, "\n;" );
				dump = FALSE;
				newline = TRUE;
			}

			byte_cnt = 0;

			if ( cycleflag )					// cycle count summary
				cycle_end( i );

			if ( skip ) {						// insert ORG statement
				if ( !newline )
					fprintf( fp, "\n;" );

				fprintf( fp, "\n\t%s\t", orgstr );
				puthex( i );
				fprintf( fp, "\n;" );
				newline = TRUE;
				skip = FALSE;					// reset skip flag
			}

			chk_ref( i );						// add label if referenced
			kcnt = 8;
			opcount = 1;
			k = pgmmem[i];						// get opcode

			if ( cputype == c_8080 ) {
				j = opttbl80[k];				// get options
				// sim/rim not in 8080
				if ( k == 0x20 || k == 0x30 ) {
					doopcode( defbstr );
					fprintf( fp, "\t" );
					kcnt = ( kcnt + 8 ) & 0x78;

					q = ( int ) pgmmem[i] & 0xff;

					if ( pgmflags[i] & PF_NAME )
						cptr = find_entry( i, name_count, name_val_index );
					else
						cptr = find_entry( q, symbol_count, sym_val_index );

					if ( cptr == NULL )
						puthex( pgmmem[i] );
					else
						kcnt += fprintf( fp, "%s", cptr );
				} else
					doopcode( mnemtbl80[k].mnem );		// output mnemonic
			} else {
				j = opttbl[k];					// get options
				doopcode( mnemtbl[k].mnem );			// output mnemonic
			}

//
// Generate operands
//
			switch ( j & ~OPT_XFER ) {
			case OPT_NONE:						// single byte - no options
				break;

			case OPT_PAR:						// should only occur if 8080/8085 mnemonics
				if ( cputype != c_8080 ) {
					fprintf( stderr, "\nZ80 PROCESSING ERROR!\n" );
					fflush( stderr );
				}

				doopcode( defbstr );
				fprintf( fp, "\t" );
				kcnt = ( kcnt + 8 ) & 0x78;

				q = ( int ) pgmmem[i] & 0xff;

				if ( pgmflags[i] & PF_NAME )
					cptr = find_entry( i, name_count, name_val_index );
				else
					cptr = find_entry( q, symbol_count, sym_val_index );

				if ( cptr == NULL )
					puthex( pgmmem[i] );
				else
					kcnt += fprintf( fp, "%s", cptr );

				break;

			case OPT_IMM2:						// 2 byte immediate data
				q = ( int ) pgmmem[i + 1] & 0xff;

				if ( pgmflags[i + 1] & PF_NAME )
					cptr = find_entry( i + 1, name_count, name_val_index );
				else
					cptr = find_entry( q, symbol_count, sym_val_index );

				if ( cptr == NULL )
					puthex( pgmmem[i + 1] );
				else
					kcnt += fprintf( fp, "%s", cptr );

				splitcheck( i + 1 );
				break;

			case OPT_PAR2:						// 2 byte parenthesized address (in, out)
				fprintf( fp, "(" );
				kcnt++;
				q = ( int ) pgmmem[i + 1] & 0xff;

				if ( pgmflags[i + 1] & PF_NAME )
					cptr = find_entry( i + 1, name_count, name_val_index );
				else
					cptr = find_entry( q, symbol_count, sym_val_index );

				if ( cptr == NULL )
					puthex( pgmmem[i + 1] );
				else
					kcnt += fprintf( fp, "%s", cptr );

				fprintf( fp, ")" );
				kcnt++;

				if ( !( k & 8 ) ) {				// if out (n),a
					if ( !upperflag )
						fprintf( fp, ",a" );
					else
						fprintf( fp, ",A" );

					kcnt += 2;
				}

				splitcheck( i + 1 );
				break;

			case OPT_DIR_IMM3:					// 3 byte immediate data
				q = ( pgmmem[i + 2] << 8 ) | pgmmem[i + 1];
				q &= WORD_MASK;

				if ( pgmflags[i + 1] & PF_NAME ) {
					pgmflags[i + 1] |= PF_2BYTE;
					cptr = find_entry( i + 1, name_count, name_val_index );
				} else if ( pgmflags[i] & PF_SYMBOL )
					cptr = find_entry( q, symbol_count, sym_val_index );
				else
					cptr = find_entry( q, label_count, lab_val_index );

				if ( cptr == NULL ) {
					if ( !upperflag )
						kcnt += fprintf( fp, "X%04x", q );
					else
						kcnt += fprintf( fp, "X%04X", q );
				} else
					kcnt += fprintf( fp, "%s", cptr );

				splitcheck( i + 1 );
				splitcheck( i + 2 );
				break;

			case OPT_PAR_IMM3:					// 3 byte parenthesized address, immediate data
				q = ( pgmmem[i + 2] << 8 ) | pgmmem[i + 1];
				q &= WORD_MASK;

				if ( pgmflags[i + 1] & PF_NAME ) {
					pgmflags[i + 1] |= PF_2BYTE;
					cptr = find_entry( i + 1, name_count, name_val_index );
				} else if ( pgmflags[i] & PF_SYMBOL )
					cptr = find_entry( q, symbol_count, sym_val_index );
				else
					cptr = find_entry( q, label_count, lab_val_index );

				if ( cptr == NULL ) {
					if ( !upperflag )
						kcnt += fprintf( fp, "(X%04x", q );
					else
						kcnt += fprintf( fp, "(X%04X", q );
				} else
					kcnt += fprintf( fp, "(%s", cptr );

				switch ( k ) {
				case 0x22:
					if ( !upperflag )
						kcnt += fprintf( fp, "),hl" );
					else
						kcnt += fprintf( fp, "),HL" );
					break;

				case 0x2a:
				case 0x3a:
					kcnt += fprintf( fp, ")" );
					break;

				case 0x32:
					if ( !upperflag )
						kcnt += fprintf( fp, "),a" );
					else
						kcnt += fprintf( fp, "),A" );
					break;
				}
				splitcheck( i + 1 );
				splitcheck( i + 2 );
				break;

			case OPT_REL2:						// 2 byte relative addressing
				q = pgmmem[i + 1] & 0xff;
				q = ( q > 0x7f ) ? q | 0xff00 : q;
				q = i + 2 + q;
				q &= WORD_MASK;
				cptr = find_entry( q, label_count, lab_val_index );

				if ( cptr == NULL ) {
					if ( !upperflag )
						kcnt += fprintf( fp, "X%04x", q );
					else
						kcnt += fprintf( fp, "X%04X", q );
				} else
					kcnt += fprintf( fp, "%s", cptr );

				splitcheck( i + 1 );
				break;

			case OPT_SPEC:						// special processing codes (dd, ed, fd)
				opcount = 2;

				switch ( k ) {
				case 0xed:
					k2 = pgmmem[i + 1];			// get 2nd byte (kept separate from
										// k, which opttbl[k] below still needs
										// to refer to the 0xed prefix itself)

					if ( edc[k2] ) {
						c = k2;
						doopcode( edtbl[( byte ) c].mnem );

						switch ( edc[k2] ) {
						case OPT_ED_2:
							break;

						case OPT_ED_IN0:	// IN0 r,(nn) - register already in mnemonic
							q = ( int ) pgmmem[i + 2] & 0xff;

							fprintf( fp, "(" );
							kcnt++;

							if ( pgmflags[i + 2] & PF_NAME )
								cptr = find_entry( i + 2, name_count, name_val_index );
							else
								cptr = find_entry( q, symbol_count, sym_val_index );

							if ( cptr == NULL )
								puthex( pgmmem[i + 2] );
							else
								kcnt += fprintf( fp, "%s", cptr );

							fprintf( fp, ")" );
							kcnt++;

							splitcheck( i + 2 );
							opcount = 3;
							break;

						case OPT_ED_OUT0:	// OUT0 (nn),r
							q = ( int ) pgmmem[i + 2] & 0xff;

							fprintf( fp, "(" );
							kcnt++;

							if ( pgmflags[i + 2] & PF_NAME )
								cptr = find_entry( i + 2, name_count, name_val_index );
							else
								cptr = find_entry( q, symbol_count, sym_val_index );

							if ( cptr == NULL )
								puthex( pgmmem[i + 2] );
							else
								kcnt += fprintf( fp, "%s", cptr );

							fprintf( fp, ")," );
							kcnt += 2;
							doopcode( regtbl[( k2 >> 3 ) & 7].mnem );

							splitcheck( i + 2 );
							opcount = 3;
							break;

						case OPT_ED_IMM:	// TST nn / TSTIO nn - plain immediate
							q = ( int ) pgmmem[i + 2] & 0xff;

							if ( pgmflags[i + 2] & PF_NAME )
								cptr = find_entry( i + 2, name_count, name_val_index );
							else
								cptr = find_entry( q, symbol_count, sym_val_index );

							if ( cptr == NULL )
								puthex( pgmmem[i + 2] );
							else
								kcnt += fprintf( fp, "%s", cptr );

							splitcheck( i + 2 );
							opcount = 3;
							break;

						case OPT_ED_STORE:
							q = ( pgmmem[i + 3] << 8 ) | pgmmem[i + 2];
							q &= WORD_MASK;

							if ( pgmflags[i + 2] & PF_NAME ) {
								pgmflags[i + 2] |= PF_2BYTE;
								cptr = find_entry( i + 2, name_count, name_val_index );
							} else if ( pgmflags[i] & PF_SYMBOL )
								cptr = find_entry( q, symbol_count, sym_val_index );
							else
								cptr = find_entry( q, label_count, lab_val_index );

							if ( cptr == NULL ) {
								if ( !upperflag )
									kcnt += fprintf( fp, "(X%04x)", q );
								else
									kcnt += fprintf( fp, "(X%04X)", q );
							} else
								kcnt += fprintf( fp, "(%s)", cptr );

							opcount = 4;
							break;

						case OPT_ED_LD_BC:
							q = ( pgmmem[i + 3] << 8 ) | pgmmem[i + 2];
							q &= WORD_MASK;

							if ( pgmflags[i + 2] & PF_NAME ) {
								pgmflags[i + 2] |= PF_2BYTE;
								cptr = find_entry( i + 2, name_count, name_val_index );
							} else if ( pgmflags[i] & PF_SYMBOL )
								cptr = find_entry( q, symbol_count, sym_val_index );
							else
								cptr = find_entry( q, label_count, lab_val_index );

							if ( cptr == NULL ) {
								if ( !upperflag )
									kcnt += fprintf( fp, "(X%04x)", q );
								else
									kcnt += fprintf( fp, "(X%04X)", q );
							} else
								kcnt += fprintf( fp, "(%s)", cptr );

							if ( !upperflag )
								kcnt += fprintf( fp, ",bc" );
							else
								kcnt += fprintf( fp, ",BC" );

							opcount = 4;
							break;

						case OPT_ED_LD_DE:
							q = ( pgmmem[i + 3] << 8 ) | pgmmem[i + 2];
							q &= WORD_MASK;

							if ( pgmflags[i + 2] & PF_NAME ) {
								pgmflags[i + 2] |= PF_2BYTE;
								cptr = find_entry( i + 2, name_count, name_val_index );
							} else if ( pgmflags[i] & PF_SYMBOL )
								cptr = find_entry( q, symbol_count, sym_val_index );
							else
								cptr = find_entry( q, label_count, lab_val_index );

							if ( cptr == NULL ) {
								if ( !upperflag )
									kcnt += fprintf( fp, "(X%04x)", q );
								else
									kcnt += fprintf( fp, "(X%04X)", q );
							} else
								kcnt += fprintf( fp, "(%s)", cptr );

							if ( !upperflag )
								kcnt += fprintf( fp, ",de" );
							else
								kcnt += fprintf( fp, ",DE" );

							opcount = 4;
							break;

						case OPT_ED_LD_SP:
							q = ( pgmmem[i + 3] << 8 ) | pgmmem[i + 2];
							q &= WORD_MASK;

							if ( pgmflags[i + 2] & PF_NAME ) {
								pgmflags[i + 2] |= PF_2BYTE;
								cptr = find_entry( i + 2, name_count, name_val_index );
							} else if ( pgmflags[i] & PF_SYMBOL )
								cptr = find_entry( q, symbol_count, sym_val_index );
							else
								cptr = find_entry( q, label_count, lab_val_index );

							if ( cptr == NULL ) {
								if ( !upperflag )
									kcnt += fprintf( fp, "(X%04x)", q );
								else
									kcnt += fprintf( fp, "(X%04X)", q );
							} else
								kcnt += fprintf( fp, "(%s)", cptr );

							if ( !upperflag )
								kcnt += fprintf( fp, ",sp" );
							else
								kcnt += fprintf( fp, ",SP" );

							opcount = 4;
							break;

						case OPT_ED_RET:		// retn or reti
							j |= 0x80;
							break;
						}
					} else {
						invalided( k2 );
						j |= 0x80;
					}
					break;

				case 0xdd:
					j |= doindex( i, 'x' );
					break;

				case 0xfd:
					j |= doindex( i, 'y' );
					break;
				}
				break;

			case OPT_SPEC2:						// cb special codes
				c = pgmmem[i + 1] & 0xff;
				temp = ( int ) ( c >> 3 ) & 0x1f;

				if ( *cbtbl[temp].mnem == '\0' ) {
					doopcode( defbstr );
					putc( '\t', fp );
					kcnt = ( kcnt + 8 ) & 0x78;
					puthex( 0xcb );
					putc( ',', fp );
					kcnt++;
					puthex( c );
					j |= 0x80;
				} else {
					doopcode( cbtbl[temp].mnem );
					doopcode( regtbl[c & 7].mnem );
				}
				break;
			}

			if ( cycleflag ) {
				while ( kcnt < TSTOP ) {
					fprintf( fp, "\t" );
					kcnt = ( kcnt + 8 ) & 0x78;
				}

				// get cycles - here would come 8080 support, would we have the appropriate tables ready
				cyc = cyctbl[pgmmem[i] & 0xff];
				cyc2 = cyc2tbl[pgmmem[i] & 0xff];

				switch ( pgmmem[i] & 0xff ) {
				case 0xed:
					cyc = edcyctbl[pgmmem[i+1] & 0xff];
					cyc2 = edcyc2tbl[pgmmem[i+1] & 0xff];
					break;

				case 0xcb:
					cyc = cbcyctbl[pgmmem[i+1] & 0xff];
					cyc2 = cyc;
					break;

				case 0xdd:
					if ( ( pgmmem[i+1] & 0xff ) == 0xcb )
						cyc = ddcbcyctbl[pgmmem[i+3] & 0xff];
					else
						cyc = ddcyctbl[pgmmem[i+1] & 0xff];

					cyc2 = cyc;
					break;

				case 0xfd:
					if ( ( pgmmem[i+1] & 0xff ) == 0xcb )
						cyc = fdcbcyctbl[pgmmem[i+3] & 0xff];
					else
						cyc = fdcyctbl[pgmmem[i+1] & 0xff];

					cyc2 = cyc;
					break;
				}

// cputype == c_8080 ? i + (opttbl80[k] & OPT_SIZE) + opcount : i + (opttbl[k] & OPT_SIZE) + opcount,

				cycle_in( i, i + ( opttbl[k] & OPT_SIZE ) + opcount, cyc, cyc2 );
			}

			if ( hexflag ) {				// do comment field
				while ( kcnt < TSTOP ) {
					fprintf( fp, "\t" );
					kcnt = ( kcnt + 8 ) & 0x78;
				}

				kcnt += fprintf( fp, "; %04X ", i );

				if ( cputype == c_8080 )
					q = ( opttbl80[k] & OPT_SIZE ) + opcount;
				else
					q = ( opttbl[k] & OPT_SIZE ) + opcount;

				for ( c = 0; c < q; c++ )
					kcnt += fprintf( fp, " %02X", pgmmem[i + c] & 0xff );

				do {
					putc( '\t', fp );
					kcnt = ( kcnt + 8 ) & 0x78;
				} while ( kcnt < ASTOP );

				for ( c = 0; c < q; c++ )
					putc( ascii( pgmmem[i + c] ), fp );
			}

			if ( pgmflags[i] & PF_ICMT )
				output_icomment( i );

			newline = FALSE;

			if ( j & OPT_XFER ) {					// if unconditional transfer
				fprintf( fp, "\n;" );				// or invalid opcode
				newline = TRUE;
			}

			if ( cputype == c_8080 )
				i = i + ( opttbl80[k] & OPT_SIZE ) + opcount;	// update location counter
			else
				i = i + ( opttbl[k] & OPT_SIZE ) + opcount;	// update location counter
		}

		if ( ( i & 0xff ) < l )
			printf( "\rPass 2 %04X", i & 0xff00 );
	}

	if ( cycleflag )			// any remaining cycle count summary
		cycle_end( PMEMSIZE + 1 );	// specifying a number above any possible
						// maximum address makes cycle_end to emit all
						// remaining sums (i would probably suffice, too (?))

	if ( byte_cnt )				// if any remaining ascii or binary,
		dump_bytes( i );		// output it now

	if ( asc_cnt )
		dump_ascii( i );

	printf( "\rPass 2 - Source generation complete" );
}			// end of Pass 2

void invalided( byte k )
{
	doopcode( defbstr );
	putc( '\t', fp );
	kcnt = ( kcnt + 8 ) & 0x78;
	puthex( 0xed );
	putc( ',', fp );
	kcnt++;
	puthex( k & 0xff );
}

int doindex( int i, char index )
{
	byte			j, k;
	int			q, v;
	char			*cptr;

	j = pgmmem[i + 1] & 0xff;	// get second byte
	k = ddcode[j];			// get flags
	q = 0;

	if ( !k ) {
		doopcode( defbstr );
		putc( '\t', fp );
		kcnt = ( kcnt + 8 ) & 0x78;
		index = ( index == 'x' ) ? 0xdd : 0xfd;
		puthex( index & 0xff );
		putc( ',', fp );
		kcnt++;
		puthex( j );
		opcount = 2;
		return ( 0x80 );
	}

	if ( upperflag )
		index = toupper( index );

	j = k >> 4;

	switch ( k & 0xf ) {
	case OPT_DD_2:
		doopcode( dd1tbl[j].mnem );
		q = 0;

		switch ( j ) {
		case 0:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c,bc", index );
			else
				kcnt += fprintf( fp, "I%c,BC", index );
			break;

		case 1:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c,de", index );
			else
				kcnt += fprintf( fp, "I%c,DE", index );
			break;

		case 2:
		case 4:
		case 6:
		case 8:
		case 7:
		case 10:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c", index );
			else
				kcnt += fprintf( fp, "I%c", index );
			break;

		case 9:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c)", index );
			else
				kcnt += fprintf( fp, "I%c)", index );
			q = 0x80;
			break;

		case 3:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c,i%c", index, index );
			else
				kcnt += fprintf( fp, "I%c,I%c", index, index );
			break;

		case 5:
			if ( !upperflag )
				kcnt += fprintf( fp, "i%c,sp", index );
			else
				kcnt += fprintf( fp, "I%c,SP", index );
			break;

		default:
			break;
		}

		opcount = 2;
		return( q );

	case OPT_DD_LOAD:
		switch ( j ) {
		case 0:
			doopcode( "INC (I" );
			kcnt += fprintf( fp, "%c+", index );
			v = ( int ) pgmmem[i + 2] & 0xff;
			cptr = find_entry( v, symbol_count, sym_val_index );

			if ( cptr == NULL )
				puthex( v );
			else
				kcnt += fprintf( fp, "%s", cptr );

			putc( ')', fp );
			kcnt++;
			break;

		case 1:
			doopcode( "DEC (I" );
			kcnt += fprintf( fp, "%c+", index );
			v = ( int ) pgmmem[i + 2] & 0xff;
			cptr = find_entry( v, symbol_count, sym_val_index );

			if ( cptr == NULL )
				puthex( v );
			else
				kcnt += fprintf( fp, "%s", cptr );

			putc( ')', fp );
			kcnt++;
			break;

		case 2:
			indxld1( i, 'b', index );
			break;

		case 3:
			indxld1( i, 'c', index );
			break;

		case 4:
			indxld1( i, 'd', index );
			break;

		case 5:
			indxld1( i, 'e', index );
			break;

		case 6:
			indxld1( i, 'h', index );
			break;

		case 7:
			indxld1( i, 'l', index );
			break;

		case 8:
			indxld2( i, 'b', index );
			break;

		case 9:
			indxld2( i, 'c', index );
			break;

		case 10:
			indxld2( i, 'd', index );
			break;

		case 11:
			indxld2( i, 'e', index );
			break;

		case 12:
			indxld2( i, 'h', index );
			break;

		case 13:
			indxld2( i, 'l', index );
			break;

		case 14:
			indxld2( i, 'a', index );
			break;

		case 15:
			indxld1( i, 'a', index );
			break;
		}

		opcount = 3;
		return( 0 );

	case OPT_DD_DIR:
		doopcode( "LD " );
		q = ( pgmmem[i + 2] & 0xff ) | ( ( pgmmem[i + 3] << 8 ) & 0xff00 );

		switch ( j ) {
		case 0:
			if ( pgmflags[i + 2] & PF_NAME ) {
				pgmflags[i + 2] |= PF_2BYTE;
				cptr = find_entry( i + 2, name_count, name_val_index );
			} else if ( pgmflags[i] & PF_SYMBOL )
				cptr = find_entry( q, symbol_count, sym_val_index );
			else
				cptr = find_entry( q, label_count, lab_val_index );

			if ( cptr == NULL ) {
				if ( !upperflag )
					kcnt += fprintf( fp, "i%c,X%04x", index, q );
				else
					kcnt += fprintf( fp, "I%c,X%04X", index, q );
			} else {
				if ( !upperflag )
					kcnt += fprintf( fp, "i%c,%s", index, cptr );
				else
					kcnt += fprintf( fp, "I%c,%s", index, cptr );
			}
			break;

		case 1:
			if ( pgmflags[i + 2] & PF_NAME ) {
				pgmflags[i + 2] |= PF_2BYTE;
				cptr = find_entry( i + 2, name_count, name_val_index );
			} else if ( pgmflags[i] & PF_SYMBOL )
				cptr = find_entry( q, symbol_count, sym_val_index );
			else
				cptr = find_entry( q, label_count, lab_val_index );

			if ( cptr == NULL ) {
				if ( !upperflag )
					kcnt += fprintf( fp, "(X%04x),i%c", q, index );
				else
					kcnt += fprintf( fp, "(X%04X),I%c", q, index );
			} else {
				if ( !upperflag )
					kcnt += fprintf( fp, "(%s),i%c", cptr, index );
				else
					kcnt += fprintf( fp, "(%s),I%c", cptr, index );
			}
			break;

		case 2:
			if ( pgmflags[i + 2] & PF_NAME ) {
				pgmflags[i + 2] |= PF_2BYTE;
				cptr = find_entry( i + 2, name_count, name_val_index );
			} else if ( pgmflags[i] & PF_SYMBOL )
				cptr = find_entry( q, symbol_count, sym_val_index );
			else
				cptr = find_entry( q, label_count, lab_val_index );

			if ( cptr == NULL ) {
				if ( !upperflag )
					kcnt += fprintf( fp, "i%c,(X%04x)", index, q );
				else
					kcnt += fprintf( fp, "I%c,(X%04X)", index, q );
			} else {
				if ( !upperflag )
					kcnt += fprintf( fp, "i%c,(%s)", index, cptr );
				else
					kcnt += fprintf( fp, "I%c,(%s)", index, cptr );
			}
			break;

		case 3:		// this applies only to ld (ix+n),data and ld (iy+n),data
			if ( !upperflag )
				kcnt += fprintf( fp, "(i%c+", index );
			else
				kcnt += fprintf( fp, "(I%c+", index );

			v = ( int ) pgmmem[i + 2] & 0xff;
			cptr = find_entry( v, symbol_count, sym_val_index );

			if ( cptr == NULL )
				puthex( v );
			else
				kcnt += fprintf( fp, "%s", cptr );

			kcnt += fprintf( fp, ")," );
			v = ( int ) pgmmem[i + 3] & 0xff;

			if ( pgmflags[i + 2] & PF_NAME )
				cptr = find_entry( i + 2, name_count, name_val_index );
			else if ( pgmflags[i] & PF_LABEL )
				cptr = find_entry( q, label_count, lab_val_index );
			else
				cptr = find_entry( v, symbol_count, sym_val_index );

			if ( cptr == NULL )
				puthex( v );
			else
				kcnt += fprintf( fp, "%s", cptr );
			break;
		}

		opcount = 4;
		return( 0 );

	case OPT_DD_ARTH:
		doopcode( dd2tbl[j].mnem );
		kcnt += fprintf( fp, "%c+", index );
		v = ( int ) pgmmem[i + 2] & 0xff;

		if ( pgmflags[i + 2] & PF_NAME )
			cptr = find_entry( i + 2, name_count, name_val_index );
		else if ( pgmflags[i] & PF_LABEL )
			cptr = find_entry( q, label_count, lab_val_index );
		else
			cptr = find_entry( v, symbol_count, sym_val_index );

		if ( cptr == NULL )
			puthex( v );
		else
			kcnt += fprintf( fp, "%s", cptr );

		putc( ')', fp );
		kcnt++;
		opcount = 3;
		return( 0 );

	case OPT_DD_CB:
		j = pgmmem[i + 3];							// get cb ext byte

		if ( j == 0x36 || ( j & 7 ) != 6 )
			return( 0 );

		if ( j < 0x40 ) {
			k = j >> 3;
			doopcode( ddcbtbl[k].mnem );
		} else {
			k = ( j >> 6 ) + 7;
			doopcode( ddcbtbl[k].mnem );
			putc( ( ( j >> 3 ) & 7 ) | 0x30, fp );
			putc( ',', fp );
			kcnt += 2;
		}

		if ( !upperflag )
			kcnt += fprintf( fp, "(i%c+", index );
		else
			kcnt += fprintf( fp, "(I%c+", index );

		v = ( int ) pgmmem[i + 2] & 0xff;

		if ( pgmflags[i + 2] & PF_NAME )
			cptr = find_entry( i + 2, name_count, name_val_index );
		else if ( pgmflags[i] & PF_LABEL )
			cptr = find_entry( q, label_count, lab_val_index );
		else
			cptr = find_entry( v, symbol_count, sym_val_index );

		if ( cptr == NULL )
			puthex( v );
		else
			kcnt += fprintf( fp, "%s", cptr );

		putc( ')', fp );
		kcnt++;
		opcount = 4;
		return( 0 );
	}

	return( 0 );
}

void indxld1( int i, char reg, char idx )
{
	char			*cptr;
	int			v;

	doopcode( "LD " );

	if ( upperflag ) {
		reg = toupper( reg );
		idx = toupper( idx );
	}

	if ( upperflag )
		kcnt += fprintf( fp, "%c,(I%c+", reg, idx );
	else
		kcnt += fprintf( fp, "%c,(i%c+", reg, idx );

	v = ( int ) pgmmem[i + 2] & 0xff;

	if ( pgmflags[i + 2] & PF_NAME )
		cptr = find_entry( i + 2, name_count, name_val_index );
	else if ( pgmflags[i] & PF_LABEL )
		cptr = find_entry( v, label_count, lab_val_index );
	else
		cptr = find_entry( v, symbol_count, sym_val_index );

	if ( cptr == NULL )
		puthex( v );
	else
		kcnt += fprintf( fp, "%s", cptr );

	putc( ')', fp );
	kcnt++;
}

void indxld2( int i, char reg, char idx )
{
	char			*cptr;
	int			v;

	doopcode( "LD " );

	if ( upperflag ) {
		reg = toupper( reg );
		idx = toupper( idx );
	}

	if ( upperflag )
		kcnt += fprintf( fp, "(I%c+", idx );
	else
		kcnt += fprintf( fp, "(i%c+", idx );

	v = ( int ) pgmmem[i + 2] & 0xff;

	if ( pgmflags[i + 2] & PF_NAME )
		cptr = find_entry( i + 2, name_count, name_val_index );
	else if ( pgmflags[i] & PF_LABEL )
		cptr = find_entry( v, label_count, lab_val_index );
	else
		cptr = find_entry( v, symbol_count, sym_val_index );

	if ( cptr == NULL )
		puthex( v );
	else
		kcnt += fprintf( fp, "%s", cptr );

	kcnt += fprintf( fp, "),%c", reg );
}

// end of pass2.c

