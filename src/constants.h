/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_CONSTANTS_H
#define TALTOS_CONSTANTS_H

#include <stdint.h>

#define FILE_A		UINT64_C(0x8080808080808080)
#define FILE_B		(FILE_A >> 1)
#define FILE_C		(FILE_A >> 2)
#define FILE_D		(FILE_A >> 3)
#define FILE_E		(FILE_A >> 4)
#define FILE_F		(FILE_A >> 5)
#define FILE_G		(FILE_A >> 6)
#define FILE_H		(FILE_A >> 7)

#define RANK_8		UINT64_C(0x00000000000000ff)
#define RANK_7		(RANK_8 << 8)
#define RANK_6		(RANK_7 << 8)
#define RANK_5		(RANK_6 << 8)
#define RANK_4		(RANK_5 << 8)
#define RANK_3		(RANK_4 << 8)
#define RANK_2		(RANK_3 << 8)
#define RANK_1		(RANK_2 << 8)

#define EDGES		(FILE_A | FILE_H | RANK_1 | RANK_8)

#define DIAG_A1H8	UINT64_C(0x8040201008040201)
#define DIAG_A8H1	UINT64_C(0x0102040810204080)
#define DIAG_C2H7	UINT64_C(0x0020100804020100)

#define BLACK_SQUARES	UINT64_C(0xaa55aa55aa55aa55)
#define WHITE_SQUARES	(~BLACK_SQUARES)

#define SQ_A1		UINT64_C(0x8000000000000000)
#define SQ_B1		(SQ_A1 >> 1)
#define SQ_C1		(SQ_A1 >> 2)
#define SQ_D1		(SQ_A1 >> 3)
#define SQ_E1		(SQ_A1 >> 4)
#define SQ_F1		(SQ_A1 >> 5)
#define SQ_G1		(SQ_A1 >> 6)
#define SQ_H1		(SQ_A1 >> 7)

#define SQ_A2		(SQ_A1 >> 8)
#define SQ_B2		(SQ_B1 >> 8)
#define SQ_C2		(SQ_C1 >> 8)
#define SQ_D2		(SQ_D1 >> 8)
#define SQ_E2		(SQ_E1 >> 8)
#define SQ_F2		(SQ_F1 >> 8)
#define SQ_G2		(SQ_G1 >> 8)
#define SQ_H2		(SQ_H1 >> 8)

#define SQ_A3		(SQ_A2 >> 8)
#define SQ_B3		(SQ_B2 >> 8)
#define SQ_C3		(SQ_C2 >> 8)
#define SQ_D3		(SQ_D2 >> 8)
#define SQ_E3		(SQ_E2 >> 8)
#define SQ_F3		(SQ_F2 >> 8)
#define SQ_G3		(SQ_G2 >> 8)
#define SQ_H3		(SQ_H2 >> 8)

#define SQ_A4		(SQ_A3 >> 8)
#define SQ_B4		(SQ_B3 >> 8)
#define SQ_C4		(SQ_C3 >> 8)
#define SQ_D4		(SQ_D3 >> 8)
#define SQ_E4		(SQ_E3 >> 8)
#define SQ_F4		(SQ_F3 >> 8)
#define SQ_G4		(SQ_G3 >> 8)
#define SQ_H4		(SQ_H3 >> 8)

#define SQ_A5		(SQ_A4 >> 8)
#define SQ_B5		(SQ_B4 >> 8)
#define SQ_C5		(SQ_C4 >> 8)
#define SQ_D5		(SQ_D4 >> 8)
#define SQ_E5		(SQ_E4 >> 8)
#define SQ_F5		(SQ_F4 >> 8)
#define SQ_G5		(SQ_G4 >> 8)
#define SQ_H5		(SQ_H4 >> 8)

#define SQ_A6		(SQ_A5 >> 8)
#define SQ_B6		(SQ_B5 >> 8)
#define SQ_C6		(SQ_C5 >> 8)
#define SQ_D6		(SQ_D5 >> 8)
#define SQ_E6		(SQ_E5 >> 8)
#define SQ_F6		(SQ_F5 >> 8)
#define SQ_G6		(SQ_G5 >> 8)
#define SQ_H6		(SQ_H5 >> 8)

#define SQ_A7		(SQ_A6 >> 8)
#define SQ_B7		(SQ_B6 >> 8)
#define SQ_C7		(SQ_C6 >> 8)
#define SQ_D7		(SQ_D6 >> 8)
#define SQ_E7		(SQ_E6 >> 8)
#define SQ_F7		(SQ_F6 >> 8)
#define SQ_G7		(SQ_G6 >> 8)
#define SQ_H7		(SQ_H6 >> 8)

#define SQ_A8		(SQ_A7 >> 8)
#define SQ_B8		(SQ_B7 >> 8)
#define SQ_C8		(SQ_C7 >> 8)
#define SQ_D8		(SQ_D7 >> 8)
#define SQ_E8		(SQ_E7 >> 8)
#define SQ_F8		(SQ_F7 >> 8)
#define SQ_G8		(SQ_G7 >> 8)
#define SQ_H8		(SQ_H7 >> 8)


#define CENTER_SQ	UINT64_C(0x0000001818000000)
#define CENTER4_SQ	UINT64_C(0x00003c3c3c3c0000)

extern uint64_t knight_pattern[64];
extern uint64_t king_pattern[64];
extern uint64_t diag_masks[64];
extern uint64_t adiag_masks[64];
extern uint64_t hor_masks[64];
extern uint64_t ver_masks[64];
extern uint64_t bishop_masks[64];
extern uint64_t rook_masks[64];

void init_constants(void);

#endif
