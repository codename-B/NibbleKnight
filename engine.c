/* SPDX-License-Identifier: MIT
 * NibbleKnight 64-bit engine. Bitboards and magic sliders. */
#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "engine.h"

enum { EMPTY = 0, WP = 1, WN, WB, WR, WQ, WK, BP_PC, BN, BB_PC, BR, BQ, BK };

/* ------------------------------------------------------------------ bitboards ---- */
typedef uint64_t BB;
static inline BB bsq(int s)  { return (BB)1 << s; }
static inline int blsb(BB a) { return __builtin_ctzll(a); }
static inline int bmsb(BB a) { return 63 - __builtin_clzll(a); }
static inline int bpop(BB a) { return __builtin_popcountll(a); }
#define FOR_BITS(s, set) for (BB _b = (set); _b && ((s) = blsb(_b), 1); _b &= _b - 1)

static const BB FILE_A = 0x0101010101010101ull, FILE_H = 0x8080808080808080ull;

static inline int ptype(int v)         { return v > WK ? v - 6 : v; }                       /* piece code to piece type 1..6 */
static inline int ep_after(int pc, int fr, int to) { return (pc == WP || pc == BP_PC) && abs(to - fr) == 16 ? (fr + to) / 2 : -1; }
static inline int max_i(int a, int b)  { return a > b ? a : b; }
static inline int cheb(int a, int b)   { return max_i(abs((a & 7) - (b & 7)), abs((a >> 3) - (b >> 3))); }
static inline int manh(int a, int b)   { return abs((a & 7) - (b & 7)) + abs((a >> 3) - (b >> 3)); }
static inline int cdist(int s)         { int f = s & 7, r = s >> 3; return max_i(f < 4 ? 3 - f : f - 4, r < 4 ? 3 - r : r - 4); }
static inline int cmanh(int s)         { int f = s & 7, r = s >> 3; return (f < 4 ? 3 - f : f - 4) + (r < 4 ? 3 - r : r - 4); }
static inline bool sq_ok(int f, int r) { return (unsigned)f < 8u && (unsigned)r < 8u; }

/* ------------------------------------------------------------------ tables & sliders ---- */
static const int VAL[7] = {0, 1, 3, 3, 5, 9, 100};
static const int NOFF[8][2] = {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
static const int DOFF[8][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
static BB B_KN[64], B_KG[64], B_PSRC[2][64], B_RAY[8][64], B_QLINE[64], PASS_W[64], PASS_B[64];
static int8_t DIR[64][64];
static bool ready;

/* Magic bitboards: multiply the blockers on a slider's lines by a per-square constant to get an index into
 * its attack table. One multiply, one load. c32_init fills the tables. */
static const uint64_t MAGIC_R[64] = {0x2080002080400010ull,0x00c0002001401000ull,0x2100110008402002ull,0x0880080081041000ull,
0x0200020020041008ull,0x2300040008010012ull,0x0c00283004008201ull,0x0180010000407a80ull,
0x0168800080400020ull,0x0010400040201000ull,0x1001002001001048ull,0x1001002408100100ull,
0x0801000408010012ull,0x4001000209000400ull,0x08a20004c8020001ull,0x2002801145002280ull,
0x0080860021004200ull,0x001000c009402002ull,0x00b0002004002800ull,0x100a808010020800ull,
0x8101010008000410ull,0x0244008002000480ull,0x0000040010810208ull,0x2000020000448534ull,
0x4104400480008033ull,0x0000810100204000ull,0x0440430900200010ull,0x4600240900100100ull,
0x0060080080040080ull,0x0001000300080400ull,0x0004084400011002ull,0x0023040200008041ull,
0x0580050043002080ull,0x0400804002802008ull,0x0001002001004010ull,0x1000200901001000ull,
0x4410800801800c00ull,0xa012003806001004ull,0x0020100104008802ull,0x0004808402000041ull,
0x0010400170898000ull,0x0080500020004004ull,0x1040408012020020ull,0x8010040008004040ull,
0x2001080100110004ull,0x0000020004008080ull,0x0021010810040002ull,0x0800008c43020024ull,
0x0000800021005100ull,0x0070201040008080ull,0x0000d04282006a00ull,0x0010014400080240ull,
0x0001080110050100ull,0x0012000810240600ull,0x0402000801040200ull,0x028100108a004100ull,
0x0050800300102045ull,0x8208210040120882ull,0x8010600101183441ull,0x020b000910006045ull,
0x0241001002480005ull,0x0081000400880241ull,0x0000009008024124ull,0x0048122980410402ull
};
static const uint64_t MAGIC_B[64] = {0x0848020822040013ull,0x8010a40085821200ull,0x0008008430840822ull,0x0808048108040000ull,
0x1304042100008104ull,0x5001012010204023ull,0x81048801b8200420ull,0x200a008084012000ull,
0x0040102001042084ull,0x840a505042428020ull,0x0000700102202920ull,0x44101c0c10800002ull,
0x0040040422000000ull,0x0180020802090202ull,0x4020020811041202ull,0x000104308c042000ull,
0x4140661002424400ull,0x0028012008010460ull,0x0188062102002a00ull,0x0014004840102008ull,
0x0105000290400002ull,0x8001022200410400ull,0x104a041918013446ull,0x008a000082008238ull,
0x04a0060008100430ull,0x0008220008820801ull,0x2508041208005010ull,0x4008080200202020ull,
0x2441001013004000ull,0x0030008060407000ull,0x4008108000420800ull,0x0012021050290100ull,
0x0210080482200500ull,0xcc01112048100480ull,0x0020402806500440ull,0x00048e0080580080ull,
0x0040102020020080ull,0x0028010440080807ull,0x4601041108008800ull,0x8040810e04104200ull,
0x901210110400088aull,0xa003080212081050ull,0x00c1004048401004ull,0x900000a014400800ull,
0x0008021040405401ull,0x4020008206002090ull,0x0004190424030100ull,0x0424008a02026250ull,
0x8004088250900040ull,0x1c00430088a04200ull,0x0001020094040001ull,0x8040210020880061ull,
0x2010040450442032ull,0x0800840850044001ull,0x0004040802140004ull,0x0004080a04222020ull,
0x8088802110022000ull,0x1081a10416114400ull,0x0205010a24060820ull,0x0000000720411080ull,
0x1008000208430400ull,0x580c026028810840ull,0x802020441020a110ull,0x12c0022401020018ull
};
static BB MG_MASK[2][64], MG_ATT[107648]; static uint32_t MG_OFF[2][64]; static uint8_t MG_SH[2][64];
static BB ray_ref(int d, int s, BB occ) {                              /* slow reference: one ray, stops at the first blocker */
    BB a = B_RAY[d][s], bl = a & occ;
    return bl ? a ^ B_RAY[d][(d & 1) ? bmsb(bl) : blsb(bl)] : a;
}
static void magic_init(void) {
    uint32_t off = 0;
    for (int t = 0; t < 2; t++) for (int s = 0; s < 64; s++) {        /* t: 0 = bishop, 1 = rook. Edge squares never block. */
        BB m = t ? ((B_RAY[4][s] | B_RAY[5][s]) & ~(FILE_A | FILE_H)) | ((B_RAY[6][s] | B_RAY[7][s]) & ~0xFF000000000000FFull)
                 : (B_RAY[0][s] | B_RAY[1][s] | B_RAY[2][s] | B_RAY[3][s]) & ~0xFF818181818181FFull;
        uint64_t magic = t ? MAGIC_R[s] : MAGIC_B[s];
        int bits = bpop(m); MG_MASK[t][s] = m; MG_SH[t][s] = (uint8_t)(64 - bits); MG_OFF[t][s] = off;
        BB sub = 0;
        do {
            BB a = 0; for (int d = 4 * t; d < 4 * t + 4; d++) a |= ray_ref(d, s, sub);
            MG_ATT[off + (sub * magic >> MG_SH[t][s])] = a;
            sub = (sub - m) & m;
        } while (sub);
        off += 1u << bits;
    }
}
static inline BB diag_att(int s, BB o) { return MG_ATT[MG_OFF[0][s] + (((o & MG_MASK[0][s]) * MAGIC_B[s]) >> MG_SH[0][s])]; }
static inline BB orth_att(int s, BB o) { return MG_ATT[MG_OFF[1][s] + (((o & MG_MASK[1][s]) * MAGIC_R[s]) >> MG_SH[1][s])]; }

static void tvm_build(const int32_t *psq);
void c32_init(void) {
    if (ready) return;
    tvm_build(0);
    for (int s = 0; s < 64; s++) {
        int f = s & 7, r = s >> 3;
        for (int i = 0; i < 8; i++)
            if (sq_ok(f + NOFF[i][0], r + NOFF[i][1])) B_KN[s] |= bsq((r + NOFF[i][1]) * 8 + f + NOFF[i][0]);
        for (int df = -1; df <= 1; df++) for (int dr = -1; dr <= 1; dr++)
            if ((df || dr) && sq_ok(f + df, r + dr)) B_KG[s] |= bsq((r + dr) * 8 + f + df);
        for (int w = 0; w < 2; w++)
            for (int df = -1, pr = w ? r - 1 : r + 1; df <= 1; df += 2) if (sq_ok(f + df, pr)) B_PSRC[w][s] |= bsq(pr * 8 + f + df);
        for (int d = 0; d < 8; d++) {
            for (int ff = f + DOFF[d][0], rr = r + DOFF[d][1]; sq_ok(ff, rr); ff += DOFF[d][0], rr += DOFF[d][1])
                B_RAY[d][s] |= bsq(rr * 8 + ff);
            B_QLINE[s] |= B_RAY[d][s];
        }
        for (int ff = f - 1; ff <= f + 1; ff++) if (sq_ok(ff, 0)) {   /* squares that must have no enemy pawns for a pawn on s to be passed */
            for (int rr = r + 1; rr < 8; rr++) PASS_W[s] |= bsq(rr * 8 + ff);
            for (int rr = r - 1; rr >= 0; rr--) PASS_B[s] |= bsq(rr * 8 + ff);
        }
    }
    for (int a = 0; a < 64; a++) for (int b = 0; b < 64; b++)
        for (int d = 0; d < 8; d++) if (B_RAY[d][a] & bsq(b)) DIR[a][b] = (int8_t)d;
    magic_init();
    ready = true;
}

typedef struct { int8_t fr, to, promo; } SM;
#define MAXM 220
static inline int add_move(SM *out, int n, int fr, int to, int promo) { out[n] = (SM){(int8_t)fr, (int8_t)to, (int8_t)promo}; return n + 1; }

/* ------------------------------------------------------------------ bitboard positions ---- */
typedef struct {
    int8_t c[64];
    BB pc[13], occ, side[2];   /* side[1] = White, side[0] = Black */
    uint32_t hl, hh;           /* Zobrist hash */
    int32_t eo, sm;            /* endgame sum and colour-mirrored static sum, both updated per move */
    int8_t n[13];              /* piece counts */
    uint8_t cr, hm;            /* castling rights (1 W O-O, 2 W O-O-O, 4 B O-O, 8 B O-O-O) and half-move clock */
} BP;

static int32_t TEO[64] = {
    -5299,-4924,-597,174,-7744,-868,326,-4372,-10487,-7984,-3121,-4429,-5150,-16908,-14068,-16284,
    -11312,-5980,-8015,-7570,-9570,-8102,-14475,-10162,-11224,-6330,-15447,-14437,-9858,-3320,-9163,-7992,
    -1118,-7263,-7306,-18581,-5781,-9596,-7145,-22205,-6716,-11167,-22287,-18170,-30610,-18226,-27912,-26012,
    -14652,5524,926,-12978,-10833,-12336,-12601,-31897,26147,-7313,-9385,-26383,-1222,-14107,450,-21960
};
static int32_t TVM[13][64], TVC[13][64];
static const uint8_t CRMASK[64] = {
    13,15,15,15,12,15,15,14, 15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,  7,15,15,15, 3,15,15,11
};

/* add a piece (d = 1) or remove it (d = -1), keeping bitboards, hashes, sums and counts in step */
static void place(BP *p, int code, int s, int d) {
    BB m = bsq(s); int w = code <= WK;
    p->pc[code] ^= m; p->side[w] ^= m; p->occ ^= m;
    p->hl ^= ZKL[code * 64 + s]; p->hh ^= ZKH[code * 64 + s];
    p->eo += d * (w ? TEO[s] : -TEO[s ^ 56]); p->sm += d * TVM[code][s]; p->n[code] += d;
}

static void bp_from(const int8_t *c, BP *p) {
    *p = (BP){0};
    memcpy(p->c, c, 64);
    for (int s = 0; s < 64; s++) if (c[s]) place(p, c[s], s, 1);
}

static inline BB pawn_att_set(BB pw, int white) {
    return white ? ((pw & ~FILE_A) << 7) | ((pw & ~FILE_H) << 9)
                 : ((pw & ~FILE_A) >> 9) | ((pw & ~FILE_H) >> 7);
}
/* square of the cheapest `side` piece that attacks sq, or -1 */
static inline int bp_least_sq(const BP *p, int sq, int side) {
    int o = side ? 0 : 6; BB x;
    if ((x = B_PSRC[side][sq] & p->pc[WP + o])) return blsb(x);
    if ((x = B_KN[sq] & p->pc[WN + o])) return blsb(x);
    BB dg = (p->pc[WB + o] | p->pc[WQ + o]) ? diag_att(sq, p->occ) : 0;
    if ((x = dg & p->pc[WB + o])) return blsb(x);
    BB og = (p->pc[WR + o] | p->pc[WQ + o]) ? orth_att(sq, p->occ) : 0;
    if ((x = og & p->pc[WR + o])) return blsb(x);
    if ((x = (dg | og) & p->pc[WQ + o])) return blsb(x);
    if ((x = B_KG[sq] & p->pc[WK + o])) return blsb(x);
    return -1;
}
static inline int bp_cheapest(const BP *p, int sq, int side) { int s = bp_least_sq(p, sq, side); return s < 0 ? 0 : VAL[ptype(p->c[s])]; }
static inline bool bp_any(const BP *p, int sq, int white) {
    int o = white ? 0 : 6;
    if ((B_PSRC[white][sq] & p->pc[WP + o]) | (B_KN[sq] & p->pc[WN + o]) | (B_KG[sq] & p->pc[WK + o])) return true;
    BB q = p->pc[WQ + o], dq = p->pc[WB + o] | q, oq = p->pc[WR + o] | q;
    return (dq && (diag_att(sq, p->occ) & dq)) || (oq && (orth_att(sq, p->occ) & oq));
}
static inline bool bp_in_check(const BP *p, int white) { return bp_any(p, blsb(p->pc[white ? WK : BK]), !white); }

static inline __attribute__((always_inline)) BB piece_att(const BP *p, int t, int s, int white) {
    switch (t) {
        case WP: return pawn_att_set(bsq(s), white);
        case WN: return B_KN[s];
        case WB: return diag_att(s, p->occ);
        case WR: return orth_att(s, p->occ);
        case WQ: return diag_att(s, p->occ) | orth_att(s, p->occ);
        default: return B_KG[s];
    }
}

/* fast make move on a copy. No undo, no hashes, no sums. Used by the move orderer. */
static inline void bp_move(BP *p, int fr, int to, int promo) {
    int a = p->c[fr], v = p->c[to], na = promo ? promo : a, w = a <= WK;
    BB F = bsq(fr), T = bsq(to);
    p->pc[a] ^= F; p->pc[na] |= T; p->side[w] ^= F | T; p->occ ^= F;
    if (v) { p->pc[v] ^= T; p->side[!w] ^= T; } else p->occ |= T;
    p->c[to] = (int8_t)na; p->c[fr] = 0;
}
static inline void bp_move_ep(BP *p, int fr, int to, int promo, int ep) {
    int a = p->c[fr];
    if (to == ep && (a == WP || a == BP_PC)) {
        int cs = to + (a == WP ? -8 : 8), v = p->c[cs]; BB C = bsq(cs);
        p->pc[v] ^= C; p->side[v <= WK] ^= C; p->occ ^= C; p->c[cs] = 0;
    }
    bp_move(p, fr, to, promo);
}

typedef struct { BB p, m, r, q, k, all; } AMAP;
static void bp_amap(const BP *p, int white, AMAP *a) {
    int o = white ? 0 : 6, s; BB m = 0, r = 0, q = 0;
    a->p = pawn_att_set(p->pc[WP + o], white);
    FOR_BITS(s, p->pc[WN + o]) m |= B_KN[s];
    FOR_BITS(s, p->pc[WB + o]) m |= diag_att(s, p->occ);
    FOR_BITS(s, p->pc[WR + o]) r |= orth_att(s, p->occ);
    FOR_BITS(s, p->pc[WQ + o]) q |= diag_att(s, p->occ) | orth_att(s, p->occ);
    a->m = m; a->r = r; a->q = q; a->k = B_KG[blsb(p->pc[WK + o])];
    a->all = a->p | m | r | q | a->k;
}
static inline int amap_hanging(const BP *p, int white, const AMAP *en, const AMAP *own) {
    int o = white ? 0 : 6; BB loose = en->all & ~own->all;
    if (p->pc[WQ + o] & (en->p | en->m | en->r | loose)) return 9;
    if (p->pc[WR + o] & (en->p | en->m | loose)) return 5;
    if ((p->pc[WB + o] | p->pc[WN + o]) & (en->p | loose)) return 3;
    if (p->pc[WP + o] & loose) return 1;
    return 0;
}

/* ------------------------------------------------------------------ evaluator (Q16) ---- */
#define ONE 65536
#define INF 0x3FFFFFFF
#define MATE (2048 * ONE)
#define MATE_STEP (ONE / 8)
#define MATE_LIM (MATE - 1024 * ONE)
#define EVAL_MAX (512 * ONE)

static inline int32_t e_of_x(int32_t x) { return (x * 16913) >> 14; }
static inline int32_t div25(int32_t x)  { return x >= 0 ? (x * 5243) >> 17 : -((-x * 5243) >> 17); }
static inline int32_t clampv(int32_t v) { return v > EVAL_MAX ? EVAL_MAX : v < -EVAL_MAX ? -EVAL_MAX : v; }
static inline int32_t tt_to(int32_t v, int ply)   { return v > MATE_LIM ? v + ply * MATE_STEP : v < -MATE_LIM ? v - ply * MATE_STEP : v; }
static inline int32_t tt_from(int32_t v, int ply) { return v > MATE_LIM ? v - ply * MATE_STEP : v < -MATE_LIM ? v + ply * MATE_STEP : v; }
static inline int32_t mate_in(int ply) { return MATE - ply * MATE_STEP; }

int c32_mop_on = 1, c32_mop_edge = ONE / 2, c32_mop_near = ONE / 6, c32_mop_mob = ONE / 4;

static const int CNT_ROW[13] = {-1, 128, 129, 130, 131, 132, -1, 133, 134, 135, 136, 137, -1};
static void tvm_build(const int32_t *psq) {
    for (int c = 1; c <= 12; c++) for (int q = 0; q < 64; q++)
        TVC[c][q] = (psq ? psq[(c - 1) * 64 + q] : TV[(c <= WK ? 0 : 64) + q]) + (CNT_ROW[c] >= 0 ? TV[CNT_ROW[c]] : 0);
    for (int c = 1; c <= 12; c++) for (int q = 0; q < 64; q++) TVM[c][q] = TVC[c > WK ? c - 6 : c + 6][q ^ 56];
}
void c32_set_value_tables(const int32_t *tv, const int32_t *psq) { c32_init(); memcpy(TV, tv, sizeof TV); tvm_build(psq); }

static int32_t tab_static(const BP *p) {
    int32_t v = TV[142]; int s;
    for (int k = 1; k <= 12; k++) FOR_BITS(s, p->pc[k]) v += TVC[k][s];
    return v;
}

/* ------------------------------------------------------------------ search state, clock & table ---- */
static int32_t whist[4096], bhist[4096];
static long c32_leaves, qs_nodes, qs_pruned, tt_probe, tt_hit, tt_cut, pvs_re, null_cut, lmr_reduced, lmr_research, pawn_hit, pawn_miss;
#ifdef C32_STATS
#define STAT(x) ((x)++)
#else
#define STAT(x) ((void)0)
#endif
static int root_depth;

#if defined(__unix__) || defined(__APPLE__)
#include <time.h>
static uint32_t host_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000u); }
uint32_t (*c32_clock_ms)(void) = host_ms;
#else
uint32_t (*c32_clock_ms)(void) = 0;
#endif
static bool c32_abort, c32_timed; static uint32_t c32_deadline, poll_n;
static inline bool time_up(void) {
    if (!c32_timed) return false;
    if (!c32_abort && (++poll_n & 1023) == 0 && (int32_t)(c32_clock_ms() - c32_deadline) > 0) c32_abort = true;
    return c32_abort;
}

static uint16_t pvm[34][34]; static int pvl[34]; static uint16_t root_pv[34]; static int root_pvn;
#define PV_EMPTY(p) (pvl[p] = (p))
#define PV_SET(p, mv) do { pvm[p][p] = (uint16_t)(mv); for (int k_ = (p) + 1; k_ < pvl[(p) + 1]; k_++) pvm[p][k_] = pvm[(p) + 1][k_]; pvl[p] = pvl[(p) + 1] > (p) + 1 ? pvl[(p) + 1] : (p) + 1; } while (0)

/* transposition table: 262,144 entries (3 MB) */
#define TT_BITS 18
typedef struct { uint32_t chk; int32_t val; uint16_t mv; int8_t left, flag; } TTE;   /* flag: 1 = exact, 2 = lower bound, 3 = upper bound */
static TTE tt[1 << TT_BITS];
int c32_tt_on = 1, c32_tt_men = 6; static bool tt_now = true;
static inline uint32_t side_l(const BP *P, int black) { return P->hl ^ (black ? 0x9E3779B9u : 0); }
static inline uint32_t side_h(const BP *P, int black) { return P->hh ^ (black ? 0x7F4A7C15u : 0); }
static inline TTE *tt_slot(const BP *P, int black, int ep, uint32_t *chk) {
    uint32_t l = side_l(P, black) ^ (ep >= 0 ? ZKL[ep] : 0) ^ (P->cr * 0x2545F491u);
    *chk = (side_h(P, black) ^ (ep >= 0 ? ZKH[ep] : 0) ^ (P->cr * 0x9E3779B1u)) | 1;
    return &tt[l & ((1u << TT_BITS) - 1)];
}
void c32_tt_clear(void) { memset(tt, 0, sizeof tt); }

int c32_lmr_full = 0, c32_lmr_min = 3, c32_lmr_depth = 0;
int c32_pvs = 1, c32_killers = 1;
static uint16_t killer[64][2];
int c32_null_on = 1, c32_null_r = 2, c32_draw_rules = 1;

/* ------------------------------------------------------------------ moves: make, unmake, generate ---- */
typedef struct { int8_t fr, to, pc, cap, capsq, np, castle; uint8_t cr, hm; } UNDO;
static inline int32_t mk(BP *p, int fr, int to, int promo, int ep, UNDO *u) {
    int a = p->c[fr], w = a <= WK, capsq = to, v = p->c[to];
    if (!v && to == ep && (a == WP || a == BP_PC)) { capsq = to + (a == WP ? -8 : 8); v = p->c[capsq]; }
    int na = promo ? promo : a;
    int32_t d = TVC[na][to] - TVC[a][fr];
    if (v) { d -= TVC[v][capsq]; place(p, v, capsq, -1); p->c[capsq] = 0; }
    place(p, a, fr, -1); place(p, na, to, 1); p->c[fr] = 0; p->c[to] = (int8_t)na;
    u->fr = (int8_t)fr; u->to = (int8_t)to; u->pc = (int8_t)a; u->cap = (int8_t)v; u->capsq = (int8_t)capsq; u->np = (int8_t)na;
    u->cr = p->cr; u->castle = 0;
    if ((a == WK || a == BK) && abs(to - fr) == 2) {
        int rf = to > fr ? fr + 3 : fr - 4, rt = to > fr ? fr + 1 : fr - 1, rk = w ? WR : BR;
        d += TVC[rk][rt] - TVC[rk][rf];
        place(p, rk, rf, -1); place(p, rk, rt, 1); p->c[rf] = 0; p->c[rt] = (int8_t)rk; u->castle = 1;
    }
    p->cr &= CRMASK[fr] & CRMASK[to];
    u->hm = p->hm; p->hm = (v || a == WP || a == BP_PC) ? 0 : (uint8_t)(p->hm < 255 ? p->hm + 1 : 255);
    return d;
}
static inline void unmk(BP *p, const UNDO *u) {
    if (u->castle) {
        int fr = u->fr, to = u->to, rf = to > fr ? fr + 3 : fr - 4, rt = to > fr ? fr + 1 : fr - 1, rk = u->pc == WK ? WR : BR;
        place(p, rk, rt, -1); place(p, rk, rf, 1); p->c[rt] = 0; p->c[rf] = (int8_t)rk;
    }
    place(p, u->np, u->to, -1); place(p, u->pc, u->fr, 1); p->c[(int)u->to] = 0; p->c[(int)u->fr] = u->pc;
    if (u->cap) { place(p, u->cap, u->capsq, 1); p->c[(int)u->capsq] = u->cap; }
    p->cr = u->cr; p->hm = u->hm;
}

/* pseudo-legal moves for `white`; `quiet` keeps only moves to empty squares (en passant counts) */
static int gen_bp(const BP *P, int white, int ep, SM *out, bool quiet) {
    const int8_t *c = P->c; int n = 0, off = white ? 0 : 6, dir = white ? 8 : -8, start = white ? 1 : 6, last = white ? 6 : 1, s, to;
    BB own = P->side[white], en = P->side[!white];
#define ADD(a, b, p) do { if (!quiet || !c[b]) n = add_move(out, n, a, b, p); } while (0)
    FOR_BITS(s, own) {
        int f = s & 7, r = s >> 3, t = c[s] - off, pq = r == last ? WQ + off : 0;
        if (t == WP) {
            int one = s + dir;
            if (!c[one]) { ADD(s, one, pq); if (r == start && !c[one + dir]) ADD(s, one + dir, 0); }
            if (f > 0 && ((en & bsq(one - 1)) || one - 1 == ep)) ADD(s, one - 1, pq);
            if (f < 7 && ((en & bsq(one + 1)) || one + 1 == ep)) ADD(s, one + 1, pq);
        } else {
            BB att = piece_att(P, t, s, white) & ~own;
            if (quiet) att &= ~en;
            FOR_BITS(to, att) ADD(s, to, 0);
            if (t == WK && (P->cr & (white ? 3 : 12)) && s == (white ? 4 : 60) && !bp_any(P, s, !white)) {
                int rk = white ? WR : BR;
                if ((P->cr & (white ? 1 : 4)) && c[s + 3] == rk && !c[s + 1] && !c[s + 2] && !bp_any(P, s + 1, !white) && !bp_any(P, s + 2, !white)) ADD(s, s + 2, 0);
                if ((P->cr & (white ? 2 : 8)) && c[s - 4] == rk && !c[s - 1] && !c[s - 2] && !c[s - 3] && !bp_any(P, s - 1, !white) && !bp_any(P, s - 2, !white)) ADD(s, s - 2, 0);
            }
        }
        if (n > MAXM - 30) break;
    }
#undef ADD
    return n;
}

static int gen_caps(const BP *P, int white, SM *out) {
    const int8_t *c = P->c; int n = 0, off = white ? 0 : 6, dir = white ? 8 : -8, last = white ? 6 : 1, s, to;
    BB en = P->side[!white];
    FOR_BITS(s, P->side[white]) {
        int t = c[s] - off, f = s & 7;
        if (t == WP) {
            int pq = (s >> 3) == last ? WQ + off : 0, one = s + dir;
            if (f > 0 && (en & bsq(one - 1))) n = add_move(out, n, s, one - 1, pq);
            if (f < 7 && (en & bsq(one + 1))) n = add_move(out, n, s, one + 1, pq);
        } else FOR_BITS(to, piece_att(P, t, s, white) & en) n = add_move(out, n, s, to, 0);
    }
    return n;
}

/* pieces in `blockers` that alone stand between square k and a slider in `sliders`:
 * own pieces -> pins; enemy pieces -> discovered checkers */
static BB line_blockers(const BP *P, int k, int sliders, int blockers) {
    BB bl = P->side[blockers], pins = 0; int e = sliders ? 0 : 6, s;
    BB dsl = P->pc[WB + e] | P->pc[WQ + e], osl = P->pc[WR + e] | P->pc[WQ + e], thru = P->occ & ~bl;
    FOR_BITS(s, (diag_att(k, thru) & dsl) | (orth_att(k, thru) & osl)) {
        int d = DIR[k][s];
        BB on = B_RAY[d][k] & ~B_RAY[d][s] & ~bsq(s) & P->occ;
        if (on && !(on & (on - 1)) && (on & bl)) pins |= on;
    }
    return pins;
}
static inline BB pinned_set(const BP *P, int white, int k) { return line_blockers(P, k, !white, white); }

/* legal moves for `white`, up to `max`. has_legal uses max = 1. */
static int legal_moves(BP *P, int white, int ep, SM *out, int max) {
    SM ms[MAXM]; int n0 = gen_bp(P, white, ep, ms, false), n = 0; UNDO u;
    for (int i = 0; i < n0 && n < max; i++) {
        mk(P, ms[i].fr, ms[i].to, ms[i].promo, ep, &u); bool ok = !bp_in_check(P, white); unmk(P, &u);
        if (ok) out[n++] = ms[i];
    }
    return n;
}
/* does `white` have a legal move? Cheap checks first, full generator last. */
static bool has_legal(BP *P, int white, int ep) {
    BB kk = P->pc[white ? WK : BK], own = P->side[white], en = P->side[!white]; int k = blsb(kk), s; UNDO u;
    if (!bp_any(P, k, !white)) {
        FOR_BITS(s, own & ~(pinned_set(P, white, k) | kk)) {
            int t = ptype(P->c[s]), one = s + (white ? 8 : -8);
            BB tg = t == WP ? (piece_att(P, WP, s, white) & en) | (P->c[one] ? 0 : bsq(one)) : piece_att(P, t, s, white) & ~own;
            if (tg) return true;
        }
    } else FOR_BITS(s, B_KG[k] & ~own) { mk(P, k, s, 0, ep, &u); bool ok = !bp_in_check(P, white); unmk(P, &u); if (ok) return true; }
    SM m[1]; return legal_moves(P, white, ep, m, 1) > 0;
}

/* ------------------------------------------------------------------ move features (orderer) ---- */
static int bp_see_loss(const BP *p0, int sq) {
    BP p = *p0; int gain[32], d = 0, side = 0, on = VAL[ptype(p.c[sq])];
    for (int s; (s = bp_least_sq(&p, sq, side)) >= 0; side = !side) {
        int t = ptype(p.c[s]);
        if (t == WK) { BP q = p; bp_move(&q, s, sq, 0); if (bp_least_sq(&q, sq, !side) >= 0) break; }
        gain[d++] = on; on = VAL[t]; bp_move(&p, s, sq, 0);
    }
    int res = 0;
    while (d--) res = max_i(gain[d] - res, 0);
    return res;
}
/* for the orderer: does Black have a legal reply (`any`), and does one of them mate (`mate`)?
 * Skips the full legality test for replies that cannot be illegal, and the check test for replies
 * that cannot give check. */
static void black_replies(const BP *p, int ep, bool *any, bool *mate) {
    SM ms[MAXM]; int n = gen_bp(p, 0, ep, ms, false); *any = *mate = false;
    int wk = blsb(p->pc[WK]), bk = blsb(p->pc[BK]);
    BB d = diag_att(wk, p->occ), o = orth_att(wk, p->occ);
    BB chk[7] = {0, B_PSRC[0][wk], B_KN[wk], d, o, d | o, 0};       /* chk[t]: squares where a piece of type t gives check */
    BB disc = line_blockers(p, wk, 0, 0), kline = bp_any(p, bk, 1) ? ~(BB)0 : pinned_set(p, 0, bk);
    for (int i = 0; i < n && !*mate; i++) {
        int fr = ms[i].fr, to = ms[i].to, t = ms[i].promo ? WQ : ptype(p->c[fr]);
        bool risky = (kline & bsq(fr)) || fr == bk || (to == ep && t == WP);
        bool may_check = (chk[t] & bsq(to)) || (disc & bsq(fr)) || (to == ep && t == WP) || (t == WK && abs(to - fr) == 2);
        if (*any && !may_check) continue;
        if (!risky && !may_check) { *any = true; continue; }
        BP q = *p; bp_move_ep(&q, fr, to, ms[i].promo, ep);
        if (!bp_in_check(&q, 0)) { *any = true; if (may_check && bp_in_check(&q, 1) && !has_legal(&q, 1, ep_after(p->c[fr], fr, to))) *mate = true; }
    }
}
/* position after a White move, on a copy */
static inline void successor(const BP *P, int ft, BP *q) {
    int fr = ft >> 6, to = ft & 63, pc = P->c[fr]; UNDO u;
    int ep = pc == WP && (fr & 7) != (to & 7) && !P->c[to] ? to : -1, promo = pc == WP && (to >> 3) == 7 ? WQ : 0;
    *q = *P; mk(q, fr, to, promo, ep, &u);
}
static uint32_t move_feat(const BP *P, int ft) {
    const int8_t *code = P->c; int fr = ft >> 6, to = ft & 63, pc = code[fr];
    uint32_t f = 0;
    if (code[to]) f |= code[to] - 6;                                       /* captured piece type */
    if (pc == WP && (fr & 7) != (to & 7) && !code[to]) f |= 1;             /* en passant */
    int moved = pc == WP && (to >> 3) == 7 ? WQ : pc;
    if (moved != pc) f |= 1 << 5;                                          /* promotion */
    if (pc == WK && fr == 4 && (to == 6 || to == 2)) f |= 1 << 6;          /* castling */
    BP p; successor(P, ft, &p);
    if (bp_any(&p, blsb(p.pc[BK]), 1)) f |= 1 << 3;
    int att = bp_cheapest(&p, to, 0);
    if (att && (!bp_cheapest(&p, to, 1) || att < VAL[moved])) f |= 1 << 4;
    bool any, mate; black_replies(&p, ep_after(pc, fr, to), &any, &mate);
    if (!any) f |= (f & (1 << 3)) ? 1 << 7 : 1 << 8;                       /* gives mate or stalemate */
    AMAP aw, ab; bp_amap(&p, 1, &aw); bp_amap(&p, 0, &ab);
    f |= (uint32_t)amap_hanging(&p, 1, &ab, &aw) << 12;
    f |= (uint32_t)amap_hanging(&p, 0, &aw, &ab) << 16;
    int sl = bp_see_loss(&p, to); f |= (uint32_t)(sl > 15 ? 15 : sl) << 20;
    if (mate) f |= 1u << 9;                                                /* allows mate in one */
    return f;
}
/* learned move score: piece layout after the move plus the tactical features of the move */
static void order_scores(const BP *P, const uint16_t *fts, int n, int32_t *out) {
    for (int i = 0; i < n; i++) {
        int ft = fts[i], s; BP q; successor(P, ft, &q);
        int32_t v = TO[143];
        FOR_BITS(s, q.side[1]) v += TO[s];
        FOR_BITS(s, q.side[0]) v += TO[64 + s];
        uint32_t f = move_feat(P, ft);
        if (f & 7) v += TO[128 + (int)(f & 7) - 1];
        for (int b = 3; b <= 8; b++) if ((f >> b) & 1) v += TO[130 + b];
        v += TO[139] * (int32_t)((f >> 12) & 15) + TO[140] * (int32_t)((f >> 16) & 15) + TO[141] * (int32_t)((f >> 20) & 15);
        if ((f >> 9) & 1) v += TO[142];
        out[i] = v;
    }
}

/* ------------------------------------------------------------------ repetitions & draws ---- */
static uint32_t rep_l[640], rep_h[640]; static int rep_hist, rep_n;
int c32_rep_on = 1, c32_tt_cut_ply = 4;
/* has this position come up before? Only plies since the last capture or pawn move can match.
 * A game-history match needs two earlier hits; a search-path match needs one. */
static inline bool rep_seen(uint32_t l, uint32_t h, int hm) {
    int k = 0, stop = max_i(rep_n - 1 - hm, 0);
    for (int i = rep_n - 1; i >= stop; i--) if (rep_l[i] == l && rep_h[i] == h && (i >= rep_hist || ++k >= 2)) return true;
    return false;
}
void c32_set_history(const int8_t *boards, const uint8_t *white_to_move, int n) {
    c32_init(); if (n > 400) { boards += 64 * (n - 400); white_to_move += n - 400; n = 400; }
    for (int i = 0; i < n; i++) { BP p; bp_from(boards + 64 * i, &p); rep_l[i] = side_l(&p, !white_to_move[i]); rep_h[i] = side_h(&p, !white_to_move[i]); }
    rep_hist = rep_n = n;
}
static inline bool insufficient(const BP *P) {
    if (P->n[WP] | P->n[BP_PC] | P->n[WR] | P->n[BR] | P->n[WQ] | P->n[BQ]) return false;
    if (P->n[WN] + P->n[WB] + P->n[BN] + P->n[BB_PC] <= 1) return true;
    if (P->n[WN] | P->n[BN]) return false;
    BB b = P->pc[WB] | P->pc[BB_PC], light = 0x55AA55AA55AA55AAull;
    return !(b & light) || !(b & ~light);
}

/* ------------------------------------------------------------------ endgame & leaf evaluation ---- */
int c32_qs_depth = 8, c32_qs_prune = 1;
#define VALQ (ONE / 3)

static void bp_mirror(const BP *p, BP *m) {   /* swap colours and mirror ranks. Copies only the fields the evaluator reads. */
    for (int k = 1; k <= 6; k++) { m->pc[k] = __builtin_bswap64(p->pc[k + 6]); m->pc[k + 6] = __builtin_bswap64(p->pc[k]); }
    m->side[1] = __builtin_bswap64(p->side[0]); m->side[0] = __builtin_bswap64(p->side[1]); m->occ = __builtin_bswap64(p->occ);
}
static int32_t eval_nq(const BP *p, int32_t v) {
    AMAP aw, ab; bp_amap(p, 1, &aw); bp_amap(p, 0, &ab);
    if (p->pc[WK] & ab.all) v += TV[138];
    return v + TV[139] * amap_hanging(p, 0, &aw, &ab) + TV[140] * amap_hanging(p, 1, &ab, &aw);
}
static int32_t eval_sym(const BP *p, int32_t S, const AMAP *aw, const AMAP *ab) {
    int hb = amap_hanging(p, 0, aw, ab), hw = amap_hanging(p, 1, ab, aw);
    int32_t a = S + (p->pc[WK] & ab->all ? TV[138] : 0) + TV[139] * hb + TV[140] * hw;
    int32_t b = p->sm + TV[142] + (p->pc[BK] & aw->all ? TV[138] : 0) + TV[139] * hw + TV[140] * hb;
    return (a - b) / 2;
}
static int32_t eval_black_to_move(const BP *p) { BP m; bp_mirror(p, &m); return -eval_nq(&m, tab_static(&m)); }

#define NEG 24

typedef struct { BB wp, bp, passw, passb; int16_t aw, ab; int8_t pw, pb, topw, topb, dbl, iso, conn, ok; } PAWNE;
static PAWNE pawn_tab[1 << 10];
static int conn_pairs(BB pass) {   /* passed pawns next to each other or diagonally adjacent. Each pair counts twice. */
    BB east = (pass << 1) & ~FILE_A;
    return 2 * (bpop(pass & east) + bpop(pass & (east << 8)) + bpop(pass & (east >> 8)));
}
static const PAWNE *pawn_entry(const BP *p) {
    BB wp = p->pc[WP], bpw = p->pc[BP_PC];
    uint32_t h = ((uint32_t)wp * 0x9E3779B1u) ^ ((uint32_t)(wp >> 32) * 0x85EBCA77u) ^ ((uint32_t)bpw * 0xC2B2AE3Du) ^ ((uint32_t)(bpw >> 32) * 0x27D4EB2Fu);
    PAWNE *e = &pawn_tab[(h >> 16) & 1023];
    if (e->ok && e->wp == wp && e->bp == bpw) { STAT(pawn_hit); return e; }
    STAT(pawn_miss);
    int s, pw = 0, pb = 0, aw = 0, ab = 0, topw = -1, topb = -1, dbl = 0, iso = 0, fw[8] = {0}, fb[8] = {0};
    BB passw = 0, passb = 0;
    FOR_BITS(s, wp)  { fw[s & 7]++; if (!(PASS_W[s] & bpw)) { int adv = (s >> 3) - 1; pw++; aw += adv * adv; passw |= bsq(s); if (topw < 0 || (s >> 3) > (topw >> 3)) topw = s; } }
    FOR_BITS(s, bpw) { fb[s & 7]++; if (!(PASS_B[s] & wp))  { int adv = 6 - (s >> 3); pb++; ab += adv * adv; passb |= bsq(s); if (topb < 0 || (s >> 3) < (topb >> 3)) topb = s; } }
    for (int ff = 0; ff < 8; ff++) {
        bool lw = (ff > 0 && fw[ff - 1]) || (ff < 7 && fw[ff + 1]), lb = (ff > 0 && fb[ff - 1]) || (ff < 7 && fb[ff + 1]);
        if (fw[ff] > 1) dbl += fw[ff] - 1;
        if (fb[ff] > 1) dbl -= fb[ff] - 1;
        if (fw[ff] && !lw) iso += fw[ff];
        if (fb[ff] && !lb) iso -= fb[ff];
    }
    *e = (PAWNE){wp, bpw, passw, passb, (int16_t)aw, (int16_t)ab, (int8_t)pw, (int8_t)pb, (int8_t)topw, (int8_t)topb,
                 (int8_t)dbl, (int8_t)iso, (int8_t)(conn_pairs(passw) - conn_pairs(passb)), 1};
    return e;
}

static inline int32_t eg_phase(const BP *p) {
    int npm = 3 * (p->n[WN] + p->n[WB] + p->n[BN] + p->n[BB_PC]) + 5 * (p->n[WR] + p->n[BR]) + 9 * (p->n[WQ] + p->n[BQ]);
    return npm >= 62 ? 0 : e_of_x(62 - npm);
}
/* walk the file from s by `step`: +1 if the first piece is a White rook, -1 if Black, else 0 */
static int rook_behind(const BP *p, int s, int step) {
    for (int q = s + step; q >= 0 && q < 64; q += step) if (p->c[q]) return p->c[q] == WR ? 1 : p->c[q] == BR ? -1 : 0;
    return 0;
}
static void eg_feats(const BP *p, int32_t *f) {
    int32_t e = eg_phase(p);
    const PAWNE *pe = pawn_entry(p);
    int pw = pe->pw, pb = pe->pb, aw = pe->aw, ab = pe->ab, s;
    int nbw = p->n[WB], nbb = p->n[BB_PC], nnw = p->n[WN], nnb = p->n[BN], nrw = p->n[WR], nrb = p->n[BR], nqw = p->n[WQ], nqb = p->n[BQ];
    memset(f, 0, NEG * sizeof *f);
    f[0] = (pw - pb) * 64; f[1] = div25((aw - ab) * 64);
    f[18] = 64 * (bpop(p->pc[WR] & 0x00FF000000000000ull) - bpop(p->pc[BR] & 0xFF00ull));   /* rooks on the seventh rank */
    f[19] = 64 * ((nbw >= 2) - (nbb >= 2));
    if (!e) return;
    int wk = blsb(p->pc[WK]), bk = blsb(p->pc[BK]), topw = pe->topw, topb = pe->topb;
    bool pawn_only_b = !(nnb | nbb | nrb | nqb), pawn_only_w = !(nnw | nbw | nrw | nqw);
    int unw = 0, unb = 0, rbp = 0, blk = 0;
    FOR_BITS(s, pe->passw) {
        if (pawn_only_b && cheb(bk, 56 + (s & 7)) > 7 - (s >> 3)) unw++;
        rbp += rook_behind(p, s, -8);
        if (p->c[s + 8] > WK) blk--;
    }
    FOR_BITS(s, pe->passb) {
        if (pawn_only_w && cheb(wk, s & 7) > (s >> 3)) unb++;
        rbp += rook_behind(p, s, 8);
        if (p->c[s - 8] && p->c[s - 8] <= WK) blk++;
    }
    int race = 0;
    if (topw >= 0) race += cheb(bk, 56 + (topw & 7)) - cheb(wk, topw);
    if (topb >= 0) race -= cheb(wk, topb & 7) - cheb(bk, topb);
    int mat = (p->n[WP] - p->n[BP_PC]) + 3 * (nnw + nbw - nnb - nbb) + 5 * (nrw - nrb) + 9 * (nqw - nqb);
    f[2] = e * (pw - pb); f[3] = div25(e * (aw - ab));
    f[4] = e * e / 64 * (cdist(bk) - cdist(wk)); f[5] = e * race; f[6] = (unw - unb) * 64; f[7] = e * mat;
    int sg = mat >= 2 ? 1 : mat <= -2 ? -1 : 0;
    int lpawns = sg > 0 ? p->n[WP] : sg < 0 ? p->n[BP_PC] : 0;
    f[8] = sg * e * cmanh(sg > 0 ? bk : wk);
    f[9] = sg * e * (7 - cheb(wk, bk));
    f[10] = sg * e * lpawns;
    f[11] = (sg && lpawns == 0 && abs(mat) <= 3) ? sg * e : 0;
    f[12] = sg * e;
    bool ocb = false;
    if (nbw == 1 && nbb == 1 && !nnw && !nnb && !nqw && !nqb) {
        int a = blsb(p->pc[WB]), b = blsb(p->pc[BB_PC]); ocb = (((a >> 3) + (a & 7)) & 1) != (((b >> 3) + (b & 7)) & 1);
    }
    bool rooks_only = nrw && nrb && !nbw && !nbb && !nnw && !nnb && !nqw && !nqb;
    f[13] = e * rbp; f[14] = e * pe->conn / 2; f[15] = e * blk; f[16] = e * pe->dbl; f[17] = e * pe->iso;
    f[20] = ocb ? e * mat : 0; f[21] = rooks_only ? e * mat : 0;
    f[22] = ocb ? 64 * (pw - pb) : 0; f[23] = rooks_only ? 64 * (pw - pb) : 0;
}

static int32_t TEG[NEG] = {
    -11790,31601,25125,16944,7564,769,55938,-5467,-9086,8237,-5620,-75632,
    90618,-6373,1825,30326,7251,118,6589,-6711,4606,5872,1632,-3554
};
int c32_eg_on = 1; static bool eo_on = true;
void c32_set_eg(const int32_t *w, int on) { memcpy(TEG, w, sizeof TEG); c32_eg_on = on; }
void c32_set_eo(const int32_t *w)  { eo_on = false; for (int i = 0; i < 64; i++) { TEO[i] = w[i]; if (w[i]) eo_on = true; } }
static int32_t mop_term(const BP *p) {
    int nw = 3 * (p->n[WN] + p->n[WB]) + 5 * p->n[WR] + 9 * p->n[WQ];
    int nb = 3 * (p->n[BN] + p->n[BB_PC]) + 5 * p->n[BR] + 9 * p->n[BQ];
    int lead = nw - nb + p->n[WP] - p->n[BP_PC], sg = 0;
    if (nb <= 3 && (p->pc[WR] | p->pc[WQ]) && (nb == 0 || lead >= 5)) sg = 1;
    else if (nw <= 3 && (p->pc[BR] | p->pc[BQ]) && (nw == 0 || lead <= -5)) sg = -1;
    if (!sg) return 0;
    int wk = blsb(p->pc[WK]), bk = blsb(p->pc[BK]), lk = sg > 0 ? bk : wk, mob = 0, q;
    FOR_BITS(q, B_KG[lk] & ~p->side[sg < 0]) if (!bp_any(p, q, sg > 0)) mob++;
    return sg * (4 * ONE + cmanh(lk) * c32_mop_edge + (14 - manh(wk, bk)) * c32_mop_near + (8 - mob) * c32_mop_mob);
}
static int32_t mop_material(const BP *p) {
    int32_t v = (p->n[WP] - p->n[BP_PC]) * (ONE / 3) + (p->n[WN] + p->n[WB] - p->n[BN] - p->n[BB_PC]) * ONE
              + (p->n[WR] - p->n[BR]) * (5 * ONE / 3) + (p->n[WQ] - p->n[BQ]) * (3 * ONE);
    int s;
    FOR_BITS(s, p->pc[WP])    v += (s >> 3) * (ONE / 16);
    FOR_BITS(s, p->pc[BP_PC]) v -= (7 - (s >> 3)) * (ONE / 16);
    return v;
}
/* endgame terms; the 24-input one is cached per position (about 30% of stand-pats reuse it) */
static int32_t eg_term(const BP *p) {
    if (!c32_eg_on) return 0;
    static struct EGC { uint32_t hl, hh; int32_t v; } egc[1 << 16];
    struct EGC *e = &egc[(p->hl ^ p->hh * 0x9E3779B1u) & 0xFFFF];
    if (e->hl != p->hl || e->hh != p->hh) {
        int32_t f[NEG], v = 0; eg_feats(p, f);
        for (int i = 0; i < NEG; i++) v += TEG[i] * f[i];
        *e = (struct EGC){p->hl, p->hh, v / 64};
    }
    return (eo_on ? p->eo * eg_phase(p) / 64 : 0) + e->v;
}
static inline int16_t cap_key(const BP *P, SM m) {
    return (int16_t)(VAL[ptype(P->c[(int)m.to])] * 16 - VAL[ptype(P->c[(int)m.fr])] + (m.promo ? 128 : 0));
}

/* quick stalemate screen for the capture search: true if the side to move surely has a legal move,
 * false if unsure (the caller then generates moves) */
static bool surely_has_move(const BP *P, int white, const AMAP *own, BB enemy_att) {
    int k = blsb(P->pc[white ? WK : BK]), o = white ? 6 : 0, a = white ? 0 : 6, q;
    BB mine = P->side[white], theirs = P->side[!white], pw = P->pc[WP + a];
    if (B_KG[k] & ~mine & ~enemy_att) return true;                                  /* the king has a free square */
    if (!(B_QLINE[k] & (P->pc[WB + o] | P->pc[WR + o] | P->pc[WQ + o])))
        return ((own->m | own->r | own->q) & ~mine) || ((white ? pw << 8 : pw >> 8) & ~P->occ) || (own->p & theirs);
    BB off = ~B_QLINE[k], pwo = pw & off;
    if (((white ? pwo << 8 : pwo >> 8) & ~P->occ) || (pawn_att_set(pwo, white) & theirs)) return true;
    FOR_BITS(q, P->pc[WN + a] & off) if (B_KN[q] & ~mine) return true;
    FOR_BITS(q, (P->pc[WB + a] | P->pc[WQ + a]) & off) if (diag_att(q, P->occ) & ~mine) return true;
    FOR_BITS(q, (P->pc[WR + a] | P->pc[WQ + a]) & off) if (orth_att(q, P->occ) & ~mine) return true;
    return false;
}
/* score after a check evasion at the last capture-search ply: mate/stalemate, then draw rules,
 * then the static eval */
static int32_t qs_static(BP *P, int white, int32_t S, int ep, int mate_ply) {
    bool ck = bp_in_check(P, white);
    if (!has_legal(P, white, ep)) return ck ? (white ? -mate_in(mate_ply) : mate_in(mate_ply)) : 0;
    if (c32_draw_rules && (insufficient(P) || P->hm >= 100)) return 0;
    return clampv(white ? eval_nq(P, S) : eval_black_to_move(P));
}
static int32_t qsearch(BP *P, int white, int32_t S, int32_t alpha, int32_t beta, int d, int ep) {
    STAT(qs_nodes);
    if (c32_draw_rules && insufficient(P)) return 0;
    AMAP aw, ab; bp_amap(P, 1, &aw); bp_amap(P, 0, &ab);
    bool chk = P->pc[white ? WK : BK] & (white ? ab.all : aw.all);
    int32_t mated = mate_in(root_depth + 1 + c32_qs_depth - d), best = white ? -mated : mated;
    if (c32_draw_rules && P->hm >= 100) return chk && !has_legal(P, white, ep) ? best : 0;
    if (!chk) {
        if (c32_draw_rules && !surely_has_move(P, white, white ? &aw : &ab, white ? ab.all : aw.all) && !has_legal(P, white, ep)) return 0;
        int32_t mop = c32_mop_on ? mop_term(P) : 0;
        best = clampv(mop ? mop + mop_material(P) : eval_sym(P, S, &aw, &ab) + eg_term(P));
        if (!d || (white ? best >= beta : best <= alpha)) return best;
        if (white) { if (best > alpha) alpha = best; } else { if (best < beta) beta = best; }
    }

    SM ms[MAXM]; int16_t key[MAXM]; UNDO u;
    int n = chk ? gen_bp(P, white, ep, ms, false) : gen_caps(P, white, ms), pw = white ? WP : BP_PC;
    if (!chk && ep >= 0) {
        int f = ep & 7, src = ep + (white ? -8 : 8);
        if (f > 0 && P->c[src - 1] == pw) n = add_move(ms, n, src - 1, ep, 0);
        if (f < 7 && P->c[src + 1] == pw) n = add_move(ms, n, src + 1, ep, 0);
    }
    for (int i = 0; i < n; i++) key[i] = P->c[(int)ms[i].to] ? cap_key(P, ms[i]) : (ms[i].to == ep && P->c[(int)ms[i].fr] == pw) ? 15 : -1000;
    int32_t stand = best;
    for (int q = 0; q < n; q++) {
        int bi = q; for (int i = q + 1; i < n; i++) if (key[i] > key[bi]) bi = i;
        SM m = ms[bi]; ms[bi] = ms[q]; ms[q] = m; int16_t tk = key[bi]; key[bi] = key[q]; key[q] = tk;
        int vic = P->c[(int)m.to], att = P->c[(int)m.fr];
        if (!chk && c32_qs_prune && !m.promo) {
            int tv = ptype(vic), heavy = P->n[WP] + P->n[BP_PC] + P->n[WR] + P->n[BR] + P->n[WQ] + P->n[BQ] - (tv == WP || tv == WR || tv == WQ);
            if (heavy > 0 || !c32_draw_rules) {     /* do not prune a capture that can lead to a dead position */
                int vv = vic ? VAL[tv] : 1;
                if (white ? stand + vv * VALQ + ONE <= alpha : stand - (vv * VALQ + ONE) >= beta) { STAT(qs_pruned); continue; }
                if (c32_qs_prune > 1 && VAL[ptype(att)] > vv && bp_any(P, m.to, !white)) { STAT(qs_pruned); continue; }
            }
        }
        int ep2 = ep_after(att, m.fr, m.to);
        int32_t S2 = S + mk(P, m.fr, m.to, m.promo, ep, &u);
        if (bp_in_check(P, white)) { unmk(P, &u); continue; }
        int32_t v = (chk && !d) ? qs_static(P, !white, S2, ep2, root_depth + 2 + c32_qs_depth - d)
                                : qsearch(P, !white, S2, alpha, beta, d - 1, ep2);
        unmk(P, &u);
        if (white) { if (v > best) best = v; if (best > alpha) alpha = best; }
        else       { if (v < best) best = v; if (best < beta)  beta = best; }
        if (alpha >= beta) break;
    }
    return best;   /* in check with no legal move: keep the starting mate score */
}

static int32_t leaf_value(BP *P, int32_t S, int32_t alpha, int32_t beta, int ep) {
    if (c32_rep_on && rep_seen(side_l(P, 0), side_h(P, 0), P->hm)) return 0;
    if (c32_draw_rules && insufficient(P)) return 0;
    if (c32_draw_rules && P->hm >= 100 && !(bp_in_check(P, 1) && !has_legal(P, 1, ep))) return 0;
    return qsearch(P, 1, S, alpha, beta, c32_qs_depth, ep);
}

/* ------------------------------------------------------------------ alpha-beta tree ---- */
/* one node function per colour. White searches at most BEAM moves; Black searches captures, then quiet
 * moves with killers. Scores are from White's side: White maximises, Black minimises. */
#define BEAM 40
static int32_t black_node(BP *P, int ep, int left, int32_t S, int32_t alpha, int32_t beta, bool allow_null);
static int32_t white_node(BP *P, int ep, int left, int32_t S, int32_t alpha, int32_t beta, bool allow_null);

/* draw rules first (dead position, 50 moves, repetition), then the node */
static int32_t node(BP *P, int white, int ep, int left, int32_t S, int32_t alpha, int32_t beta) {
    if (c32_draw_rules && insufficient(P)) return 0;
    if (c32_draw_rules && P->hm >= 100) {
        if (!bp_in_check(P, white) || has_legal(P, white, ep)) return 0;
        return white ? -mate_in(root_depth - left + 1) : mate_in(root_depth - left + 1);
    }
    if (c32_rep_on) {
        uint32_t l = side_l(P, !white), h = side_h(P, !white);
        if (rep_seen(l, h, P->hm)) return 0;
        rep_l[rep_n] = l; rep_h[rep_n] = h; rep_n++;
    }
    int32_t v = white ? white_node(P, ep, left, S, alpha, beta, true) : black_node(P, ep, left, S, alpha, beta, true);
    if (c32_rep_on) rep_n--;
    return v;
}

/* stable sort by key, highest first (keys <= 0 keep their order at the end); table move goes first */
static inline void sort_moves(const SM *ms, int n, const int32_t *key, uint8_t *ord, int ttm) {
    uint8_t zer[MAXM]; int np = 0, nz = 0;
    for (int i = 0; i < n; i++) {
        if (key[i] <= 0) { zer[nz++] = (uint8_t)i; continue; }
        int j = np++; while (j > 0 && key[ord[j - 1]] < key[i]) { ord[j] = ord[j - 1]; j--; } ord[j] = (uint8_t)i;
    }
    memcpy(ord + np, zer, nz);
    if (ttm >= 0) for (int q = 0; q < n; q++) if (ms[ord[q]].fr * 64 + ms[ord[q]].to == ttm) {
        uint8_t t = ord[q]; memmove(ord + 1, ord, q); ord[0] = t; break;
    }
}
/* transposition table probe and store. Probe returns true on a cutoff and sets *val. */
static bool tt_lookup(const BP *P, int black, int ep, int ply, int left, int32_t alpha, int32_t beta, TTE **te, uint32_t *chk, int *ttm, int32_t *val) {
    *te = 0; *ttm = -1;
    if (!tt_now || left < 2) return false;
    *te = tt_slot(P, black, ep, chk); STAT(tt_probe);
    if ((*te)->chk != *chk) return false;
    STAT(tt_hit); *ttm = (*te)->mv;
    int32_t tv = tt_from((*te)->val, ply), f = (*te)->flag;
    if (ply > c32_tt_cut_ply && P->hm < 80 && (*te)->left >= left && (f == 1 || (f == 2 && tv >= beta) || (f == 3 && tv <= alpha))) { STAT(tt_cut); *val = tv; return true; }
    return false;
}
static inline void tt_store(TTE *te, uint32_t chk, int32_t best, int ply, int bestm, int left, int32_t alpha0, int32_t beta0) {
    if (!te || c32_abort) return;
    *te = (TTE){chk, tt_to(best, ply), (uint16_t)bestm, (int8_t)left, (int8_t)(best <= alpha0 ? 3 : best >= beta0 ? 2 : 1)};
}

static int32_t black_node(BP *P, int ep, int left, int32_t S, int32_t alpha, int32_t beta, bool allow_null) {
    int ply = root_depth - left + 1; PV_EMPTY(ply);
    if (time_up()) return 0;
    const int8_t *c = P->c; int32_t best = INF, alpha0 = alpha, beta0 = beta, v; UNDO u;
    TTE *te; uint32_t chk; int ttm, bestm = 0, legal = 0;
    if (tt_lookup(P, 1, ep, ply, left, alpha, beta, &te, &chk, &ttm, &v)) return v;
    int kb = blsb(P->pc[BK]);
    bool incheck = bp_any(P, kb, 1);
    if (allow_null && c32_null_on && !incheck && left >= 3 && alpha > -INF / 2 && (P->n[BN] + P->n[BB_PC] + P->n[BR] + P->n[BQ]) > 0 && ply > 1) {
        int nl = left - 1 - c32_null_r;
        if (nl <= 0) { c32_leaves++; v = leaf_value(P, S, alpha, alpha + 1, -1); }
        else v = white_node(P, -1, nl, S, alpha, alpha + 1, false);
        if (v <= alpha && !c32_abort) { STAT(null_cut); return v; }
    }
    BB kline = incheck ? ~(BB)0 : pinned_set(P, 0, kb);
    SM ms[MAXM]; uint8_t ord[MAXM]; int32_t key[MAXM];
    for (int stage = 1; stage <= 2; stage++) {
        int n = stage == 1 ? gen_caps(P, 0, ms) : gen_bp(P, 0, ep, ms, true);
        for (int i = 0; i < n; i++) {
            int vic = c[(int)ms[i].to]; uint16_t ft = (uint16_t)(ms[i].fr * 64 + ms[i].to);
            key[i] = vic ? 1000000 + 1000 * VAL[ptype(vic)] : bhist[ft];
            if (c32_killers && stage == 2) { if (ft == killer[ply][0]) key[i] = 900000; else if (ft == killer[ply][1]) key[i] = 800000; }
        }
        sort_moves(ms, n, key, ord, ttm);
        for (int q = 0; q < n; q++) {
            SM m = ms[ord[q]]; int fr = m.fr, to = m.to, pc = c[fr], capt = c[to];
            bool risky = (kline & bsq(fr)) || fr == kb || (to == ep && pc == BP_PC);   /* else known legal */
            int32_t S2 = S + mk(P, fr, to, m.promo, ep, &u);
            if (risky && bp_in_check(P, 0)) { unmk(P, &u); continue; }
            legal++; PV_EMPTY(ply + 1);
            int ep2 = ep_after(pc, fr, to); bool done = false;
            if (left == 1) {
                c32_leaves++;
                if (c32_pvs && legal > 1 && alpha + 1 < beta) {
                    v = leaf_value(P, S2, beta - 1, beta, ep2);
                    if (v < beta && v > alpha) v = leaf_value(P, S2, alpha, beta, ep2);
                } else v = leaf_value(P, S2, alpha, beta, ep2);
            } else {
                if (c32_lmr_full && root_depth > c32_lmr_depth && left >= c32_lmr_min && legal > c32_lmr_full && !capt && !incheck && m.promo == 0 && !bp_in_check(P, 1)) {
                    STAT(lmr_reduced);
                    v = left - 3 <= 0 ? leaf_value(P, S2, alpha, beta, ep2) : node(P, 1, ep2, left - 3, S2, alpha, beta);
                    done = v >= beta;
                    if (!done) STAT(lmr_research);
                }
                if (!done) {
                    if (c32_pvs && legal > 1 && alpha + 1 < beta) {
                        v = node(P, 1, ep2, left - 1, S2, beta - 1, beta);
                        if (v < beta && !c32_abort) { STAT(pvs_re); v = node(P, 1, ep2, left - 1, S2, alpha, beta); }
                    } else v = node(P, 1, ep2, left - 1, S2, alpha, beta);
                }
            }
            unmk(P, &u);
            if (v < best) { best = v; bestm = fr * 64 + to; PV_SET(ply, bestm); }
            if (best < beta) beta = best;
            if (beta <= alpha) {
                uint16_t ft = (uint16_t)(fr * 64 + to);
                if (!capt) {
                    bhist[ft] += left * left;
                    if (c32_killers && killer[ply][0] != ft) { killer[ply][1] = killer[ply][0]; killer[ply][0] = ft; }
                }
                goto done;
            }
        }
    }
    if (!legal) return incheck ? mate_in(ply) : 0;
done:
    tt_store(te, chk, best, ply, bestm, left, alpha0, beta0);
    return best;
}

static int32_t white_node(BP *P, int ep, int left, int32_t S, int32_t alpha, int32_t beta, bool allow_null) {
    if (left < 2) left = 2;
    int ply = root_depth - left + 1; PV_EMPTY(ply);
    if (time_up()) return 0;
    const int8_t *c = P->c; int32_t best = -INF, alpha0 = alpha, beta0 = beta, v; UNDO u;
    TTE *te; uint32_t chk; int ttm, bestm = 0, legal = 0;
    if (tt_lookup(P, 0, ep, ply, left, alpha, beta, &te, &chk, &ttm, &v)) return v;
    int kw = blsb(P->pc[WK]);
    bool incheck = bp_any(P, kw, 0);
    if (allow_null && c32_null_on && !incheck && left >= 3 && beta < INF / 2 && (P->n[WN] + P->n[WB] + P->n[WR] + P->n[WQ]) > 0) {
        int nl = left - 1 - c32_null_r;
        v = black_node(P, -1, nl <= 1 ? 1 : nl, S, beta - 1, beta, false);
        if (v >= beta && !c32_abort) { STAT(null_cut); return v; }
    }
    BB kline = incheck ? ~(BB)0 : pinned_set(P, 1, kw);
    SM ms[MAXM]; uint8_t ord[MAXM]; int32_t key[MAXM]; uint16_t fts[MAXM];
    int n = gen_bp(P, 1, ep, ms, false);
    if (n > BEAM) {   /* rare case: keep only legal moves, so the beam counts legal moves */
        int k = 0;
        for (int i = 0; i < n; i++) { mk(P, ms[i].fr, ms[i].to, ms[i].promo, ep, &u); if (!bp_in_check(P, 1)) ms[k++] = ms[i]; unmk(P, &u); }
        n = k;
    }
    for (int i = 0; i < n; i++) fts[i] = (uint16_t)(ms[i].fr * 64 + ms[i].to);
    if (n > BEAM) {   /* still over BEAM: the learned orderer picks which to search (keys made positive) */
        order_scores(P, fts, n, key); int32_t lo = key[0];
        for (int i = 1; i < n; i++) if (key[i] < lo) lo = key[i];
        for (int i = 0; i < n; i++) key[i] -= lo - 1;
    }
    else for (int i = 0; i < n; i++) { int vic = c[(int)ms[i].to]; key[i] = vic ? 1000000 + 1000 * VAL[ptype(vic)] - VAL[(int)c[(int)ms[i].fr]] : whist[fts[i]]; }
    sort_moves(ms, n, key, ord, ttm);
    for (int q = 0; q < n && legal < BEAM; q++) {
        SM m = ms[ord[q]]; int fr = m.fr, to = m.to, pc = c[fr], capt = c[to];
        bool risky = (kline & bsq(fr)) || fr == kw || (to == ep && pc == WP);   /* else known legal */
        int32_t S2 = S + mk(P, fr, to, m.promo, ep, &u);
        if (risky && bp_in_check(P, 1)) { unmk(P, &u); continue; }
        legal++;
        int ep2 = ep_after(pc, fr, to); bool done = false;
        if (c32_lmr_full && root_depth > c32_lmr_depth && left >= c32_lmr_min + 1 && legal > c32_lmr_full && !capt && !incheck && m.promo == 0 && !bp_in_check(P, 0)) {
            STAT(lmr_reduced);
            v = node(P, 0, ep2, left - 3, S2, alpha, alpha + 1);
            done = v <= alpha;
            if (!done) STAT(lmr_research);
        }
        if (!done) {
            if (c32_pvs && legal > 1 && alpha + 1 < beta) {
                v = node(P, 0, ep2, left - 1, S2, alpha, alpha + 1);
                if (v > alpha && v < beta && !c32_abort) { STAT(pvs_re); v = node(P, 0, ep2, left - 1, S2, alpha, beta); }
            } else v = node(P, 0, ep2, left - 1, S2, alpha, beta);
        }
        unmk(P, &u);
        if (v > best) { best = v; bestm = fr * 64 + to; PV_SET(ply, bestm); }
        if (best > alpha) alpha = best;
        if (alpha >= beta) { if (!capt) whist[fr * 64 + to] += left * left; break; }
    }
    if (!legal) return incheck ? -mate_in(ply) : 0;
    tt_store(te, chk, best, ply, bestm, left, alpha0, beta0);
    return best;
}

/* ------------------------------------------------------------------ book & root control ---- */
static uint32_t book_hits;
int c32_root_cr = 0, c32_root_hm = 0;
/* Opening book: 5 bytes per position, a sorted 32-bit key plus the rank of the book move in the legal
 * move list (sorted by from*64+to). The key mixes in castling rights and the en passant square, since both
 * change the legal moves. Pass the full list from c32_legal_moves. A false key match (about 1 in 10^6)
 * still picks a legal move, just a weaker one. */
int c32_book(const int8_t *code, const uint16_t *moves, int n) {
    uint32_t h = (uint32_t)c32_root_cr * 0x9E3779B1u;
    for (int s = 0; s < 64; s++) if (code[s]) h ^= ZKH[code[s] * 64 + s];
    for (int m = 0; m < n; m++) {   /* en passant: a pawn capture to an empty square */
        int fr = moves[m] >> 6, to = moves[m] & 63;
        if (code[fr] == WP && (fr & 7) != (to & 7) && !code[to]) { h ^= ZKH[to]; break; }
    }
    int lo = 0, hi = BOOK_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (BOOKH[mid] == h) {
            for (int m = 0; m < n; m++) {
                int rank = 0; for (int j = 0; j < n; j++) rank += moves[j] < moves[m];
                if (rank == BOOKM[mid]) { book_hits++; return m; }
            }
            return -1;
        }
        if (BOOKH[mid] < h) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

void c32_set_castling(int rights) { c32_root_cr = rights & 15; }
void c32_set_halfmove(int hm)     { c32_root_hm = hm < 0 ? 0 : hm > 255 ? 255 : hm; }

static int rs_best, c32_last_depth;
int c32_soft_pct = 40, c32_min_depth = 0;

/* search the first BEAM moves of `ord`, scored 12 * value + one[m]. Returns the best in search order,
 * or -1 on time-out. An equal null-window score is a bound, not a tie. */
static int root_search(const int8_t *code, const uint16_t *fts, int n, int depth, const int32_t *one, const int *ord, int32_t *val) {
    BP root; bp_from(code, &root); root.cr = (uint8_t)c32_root_cr; root.hm = (uint8_t)c32_root_hm;
    int32_t bests = -INF; rs_best = -1; root_depth = depth;
    for (int m = 0; m < n; m++) val[m] = -INF;
    for (int q = 0; q < n && q < BEAM; q++) {
        int m = ord[q], ft = fts[m], ep1 = ep_after(code[ft >> 6], ft >> 6, ft & 63);
        BP R; successor(&root, ft, &R);
        int32_t S = tab_static(&R), v, a = -INF;
        if (rs_best >= 0) { int32_t x = bests - one[m]; a = x / 12 - (x % 12 < 0); }   /* floor((bests - one) / 12) */
        if (c32_pvs && rs_best >= 0) {
            v = node(&R, 0, ep1, depth - 1, S, a, a + 1);
            if (v > a && !c32_abort) { STAT(pvs_re); v = node(&R, 0, ep1, depth - 1, S, a, INF); }
        } else v = node(&R, 0, ep1, depth - 1, S, a, INF);
        if (c32_abort) return -1;
        val[m] = v;
        if (rs_best < 0 || 12 * v + one[m] > bests) {
            bests = 12 * v + one[m]; rs_best = m;
            root_pv[0] = (uint16_t)ft; root_pvn = 1; for (int k = 2; k < pvl[2]; k++) root_pv[root_pvn++] = pvm[2][k];
        }
    }
    return rs_best;
}

static int32_t one_[MAXM], val_[MAXM];
static void sort_desc(int *ord, int n, const int32_t *key) {   /* stable insertion sort of 0..n-1, highest key first */
    for (int i = 0; i < n; i++) ord[i] = i;
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && key[ord[j]] > key[ord[j - 1]]; j--) { int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t; }
}
static void root_prepare(const int8_t *code, const uint16_t *fts, int n, int *ord) {
    memset(bhist, 0, sizeof bhist); memset(whist, 0, sizeof whist); memset(killer, 0, sizeof killer);
    BP R; bp_from(code, &R); order_scores(&R, fts, n, one_);
    int men = 0; for (int i = 0; i < 64; i++) men += code[i] != 0;
    tt_now = c32_tt_on && men > c32_tt_men;
    if (c32_mop_on && mop_term(&R)) for (int i = 0; i < n; i++) one_[i] /= 16;
    sort_desc(ord, n, one_);
}
/* book first, then root ordering; returns the book move or -1 */
static int root_begin(const int8_t *code, const uint16_t *fts, int n, int *ord, int use_book) {
    c32_init(); root_pvn = 0; c32_last_depth = 0;
    if (use_book) { int b = c32_book(code, fts, n); if (b >= 0) { root_pv[0] = fts[b]; root_pvn = 1; return b; } }
    root_prepare(code, fts, n, ord);
    return -1;
}
int c32_choose(const int8_t *code, const uint16_t *fts, int n, int depth, int use_book) {
    int ord[MAXM], b = root_begin(code, fts, n, ord, use_book);
    return b >= 0 ? b : root_search(code, fts, n, depth, one_, ord, val_);
}
/* iterative deepening, two plies at a time. On time-out, keep the unfinished step's best if it has one.
 * root_pv holds the PV of the returned move. */
int c32_choose_timed(const int8_t *code, const uint16_t *fts, int n, int ms, int max_depth, int use_book) {
    int ord[MAXM], best = root_begin(code, fts, n, ord, use_book);
    if (best >= 0) return best;
    if (!c32_clock_ms) return root_search(code, fts, n, 4, one_, ord, val_);
    uint32_t t0 = c32_clock_ms(), tprev = 0; c32_deadline = t0 + (uint32_t)ms;
    for (int d = 2; d <= max_depth; d += 2) {
        uint32_t ts = c32_clock_ms();
        c32_timed = d > c32_min_depth;
        if (root_search(code, fts, n, d, one_, ord, val_) < 0) { if (rs_best >= 0) best = rs_best; break; }
        best = rs_best; c32_last_depth = d;
        int32_t sc[MAXM]; for (int m = 0; m < n; m++) sc[m] = val_[m] > -INF / 2 ? 12 * val_[m] + one_[m] : -INF;
        sort_desc(ord, n, sc);
        uint32_t now = c32_clock_ms(), tn = now - ts;
        uint32_t ratio = (tprev > 0 && tn > tprev) ? (tn + tprev - 1) / tprev : 6; if (ratio < 3) ratio = 3; tprev = tn ? tn : 1;
        if (c32_soft_pct ? (now - t0) * 100 > (uint32_t)ms * (uint32_t)c32_soft_pct : (now - t0) + tn * ratio > (uint32_t)ms) break;
    }
    c32_timed = c32_abort = false;
    return best < 0 ? 0 : best;
}

int c32_open_pct = 70, c32_mid_pct = 100, c32_end_pct = 160;
int c32_phase_ms(const int8_t *code, int base) {
    int k = 0; for (int s = 0; s < 64; s++) { int t = ptype(code[s]); k += t > WP && t < WK; }
    return base * (k >= 10 ? c32_open_pct : k >= 6 ? c32_mid_pct : c32_end_pct) / 100;
}

/* ------------------------------------------------------------------ stats & public API ---- */
int      c32_last_search_depth(void) { return c32_last_depth; }
long     c32_leaf_count(void)        { return c32_leaves; }
long     c32_qs_count(void)          { return qs_nodes; }
long     c32_qs_pruned(void)         { return qs_pruned; }
long     c32_pawn_stats(long *miss)  { *miss = pawn_miss; return pawn_hit; }
int      c32_eg_count(void)          { return NEG; }
void     c32_tt_stats(long *p, long *h, long *c) { *p = tt_probe; *h = tt_hit; *c = tt_cut; }
int      c32_pv(uint16_t *out)       { memcpy(out, root_pv, root_pvn * sizeof *out); return root_pvn; }
long     c32_lmr_stats(long *re)     { *re = lmr_research; return lmr_reduced; }
long     c32_pvs_stats(void)         { return pvs_re; }
long     c32_null_stats(void)        { return null_cut; }
uint32_t c32_book_hits(void)         { return book_hits; }

int32_t c32_probe(const int8_t *code, int32_t *f) {
    c32_init(); BP P; bp_from(code, &P); eg_feats(&P, f);
    return (eval_nq(&P, tab_static(&P)) + eval_black_to_move(&P)) / 2;
}
int32_t c32_stand(const int8_t *code) {
    c32_init(); BP P; bp_from(code, &P); int32_t m = c32_mop_on ? mop_term(&P) : 0;
    return m ? m + mop_material(&P) : (eval_nq(&P, tab_static(&P)) + eval_black_to_move(&P)) / 2 + eg_term(&P);
}
int c32_legal_moves(const int8_t *code, int ep, int castling, uint16_t *out) {
    c32_init();
    BP P; bp_from(code, &P); P.cr = (uint8_t)(castling & 3);
    SM ms[MAXM]; int n = legal_moves(&P, 1, ep, ms, MAXM);
    for (int i = 0; i < n; i++) out[i] = (uint16_t)(ms[i].fr * 64 + ms[i].to);
    return n;
}
