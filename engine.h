/* SPDX-License-Identifier: MIT
 * NibbleKnight 64-bit engine: public API. All public names start with c32_. */
#ifndef NIBBLEKNIGHT64_ENGINE_H
#define NIBBLEKNIGHT64_ENGINE_H
#include <stdint.h>
void c32_init(void);
/* code[64]: a1 = 0 .. h8 = 63. 0 = empty, 1-6 = White PNBRQK, 7-12 = Black PNBRQK. White to move.
 * moves: legal White moves as from*64+to. Each function returns the index of the chosen move. */
int c32_choose(const int8_t *code, const uint16_t *moves, int n, int depth, int use_book);
int c32_choose_timed(const int8_t *code, const uint16_t *moves, int n, int ms, int max_depth, int use_book);
int c32_legal_moves(const int8_t *code, int ep, int castling, uint16_t *out);
int c32_book(const int8_t *code, const uint16_t *moves, int n);
int c32_pv(uint16_t *out);
int c32_last_search_depth(void);
int c32_phase_ms(const int8_t *code, int base);
/* game state that the board does not hold */
void c32_set_history(const int8_t *boards, const uint8_t *white_to_move, int n);   /* earlier positions, for repetition checks */
void c32_set_castling(int rights);                                                   /* 1 K 2 Q 4 k 8 q */
void c32_set_halfmove(int hm);
void c32_tt_clear(void);
/* set evaluation tables at run time (for tuning) */
void c32_set_value_tables(const int32_t *tv, const int32_t *psq);
void c32_set_eg(const int32_t *w, int on); int c32_eg_count(void);
void c32_set_eo(const int32_t *w);
int32_t c32_probe(const int8_t *code, int32_t *f);
int32_t c32_stand(const int8_t *code);
/* tuning settings. The defaults are the tested values. */
extern int c32_pvs, c32_killers, c32_null_on, c32_null_r, c32_draw_rules, c32_rep_on, c32_eg_on, c32_mop_on;
extern int c32_qs_depth, c32_qs_prune, c32_tt_on, c32_tt_men, c32_tt_cut_ply;
extern int c32_lmr_full, c32_lmr_min, c32_lmr_depth, c32_soft_pct, c32_min_depth;
extern int c32_open_pct, c32_mid_pct, c32_end_pct, c32_root_cr, c32_root_hm;
extern uint32_t (*c32_clock_ms)(void);
/* counters. Only the leaf count is active without -DC32_STATS. */
long c32_leaf_count(void); long c32_qs_count(void); long c32_qs_pruned(void); long c32_pawn_stats(long *miss);
void c32_tt_stats(long *p, long *h, long *c); long c32_lmr_stats(long *re); long c32_pvs_stats(void); long c32_null_stats(void);
uint32_t c32_book_hits(void);
#endif
