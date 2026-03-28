#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "Galleta"
#define MAX_G 10

typedef struct {
    const char* title;
    const char* rules1;
    const char* rules2;
    const char* rules3;
    const char* start;
    const char* ctrl_title;
    const char* ctrl1;
    const char* ctrl2;
    const char* ctrl3;
    const char* ctrl4;
    const char* ctrl_ok;
    const char* mode_title;
    const char* mode_1p;
    const char* mode_2p;
    const char* diff_title;
    const char* diff_easy;
    const char* diff_std;
    const char* diff_hard;
    const char* who_first;
    const char* me_first;
    const char* flip_first;
    const char* p1_first;
    const char* p2_first;
    const char* your_turn;
    const char* flip_turn;
    const char* p1_turn;
    const char* p2_turn;
    const char* you_win;
    const char* you_lose;
    const char* tie;
    const char* p1_wins;
    const char* p2_wins;
    const char* new_game;
    const char* squares;
} Lang;

static const Lang LANG_ES = {
    .title      = "GALLETA",
    .rules1     = "Pon lineas entre puntos.",
    .rules2     = "Cierra cuadros = punto.",
    .rules3     = "Turno extra al cerrar.",
    .start      = "OK = Siguiente",
    .ctrl_title = "Controles",
    .ctrl1      = "Flechas: mover cursor",
    .ctrl2      = "OK: poner linea",
    .ctrl3      = "OK (1s): girar linea",
    .ctrl4      = "Back: salir",
    .ctrl_ok    = "OK = Jugar",
    .mode_title = "Modo de juego",
    .mode_1p    = "  1 Jugador",
    .mode_2p    = "  2 Jugadores",
    .diff_title = "Dificultad",
    .diff_easy  = "  Facil",
    .diff_std   = "  Estandar",
    .diff_hard  = "  Dificil",
    .who_first  = "Quien empieza?",
    .me_first   = "  Yo empiezo",
    .flip_first = "  Flipper empieza",
    .p1_first   = "  Jugador 1",
    .p2_first   = "  Jugador 2",
    .your_turn  = "Tu turno",
    .flip_turn  = "Flipper...",
    .p1_turn    = "Turno J1",
    .p2_turn    = "Turno J2",
    .you_win    = "Ganaste!",
    .you_lose   = "Flipper gana!",
    .tie        = "Empate!",
    .p1_wins    = "Jugador 1 gana!",
    .p2_wins    = "Jugador 2 gana!",
    .new_game   = "OK=nuevo  Back=menu",
    .squares    = "cuadros",
};

static const Lang LANG_EN = {
    .title      = "GALLETA",
    .rules1     = "Place lines between dots.",
    .rules2     = "Close a square = point.",
    .rules3     = "Extra turn when closing.",
    .start      = "OK = Next",
    .ctrl_title = "Controls",
    .ctrl1      = "Arrows: move cursor",
    .ctrl2      = "OK: place line",
    .ctrl3      = "OK (1s): rotate line",
    .ctrl4      = "Back: exit",
    .ctrl_ok    = "OK = Play",
    .mode_title = "Game mode",
    .mode_1p    = "  1 Player",
    .mode_2p    = "  2 Players",
    .diff_title = "Difficulty",
    .diff_easy  = "  Easy",
    .diff_std   = "  Standard",
    .diff_hard  = "  Hard",
    .who_first  = "Who goes first?",
    .me_first   = "  I go first",
    .flip_first = "  Flipper goes first",
    .p1_first   = "  Player 1",
    .p2_first   = "  Player 2",
    .your_turn  = "Your turn",
    .flip_turn  = "Flipper...",
    .p1_turn    = "P1 turn",
    .p2_turn    = "P2 turn",
    .you_win    = "You win!",
    .you_lose   = "Flipper wins!",
    .tie        = "Tie!",
    .p1_wins    = "Player 1 wins!",
    .p2_wins    = "Player 2 wins!",
    .new_game   = "OK=new  Back=menu",
    .squares    = "squares",
};

typedef enum {
    ScnLang,
    ScnRules,
    ScnControls,
    ScnMode,
    ScnDiff,
    ScnFirst,
    ScnGame,
    ScnResult,
} Screen;

typedef struct {
    Screen      scn;
    bool        cells[MAX_G][MAX_G];
    bool        h_edge[MAX_G + 1][MAX_G];
    bool        v_edge[MAX_G][MAX_G + 1];
    int         owner[MAX_G][MAX_G];
    int         gw, gh, cs;
    int         cr, cc;
    bool        cur_h;          // cursor orientation: true=horizontal
    int         turn;
    int         s1, s2;
    int         total, claimed;
    int         menu_sel;
    bool        two_player;
    int         difficulty;     // 0=easy, 1=standard, 2=hard
    bool        p1_starts;
    uint32_t    ai_timer;
    bool        ai_pending;
    bool        running;
    const Lang* L;

    FuriMutex*        mutex;
    ViewPort*         view_port;
    Gui*              gui;
    FuriMessageQueue* queue;
} Galleta;

// ─── Edge helpers ─────────────────────────────────────────────────────────────

static bool has_top(Galleta* g, int r, int c) {
    return g->h_edge[r][c] || (r == 0 || !g->cells[r - 1][c]);
}
static bool has_bot(Galleta* g, int r, int c) {
    return g->h_edge[r + 1][c] || (r >= g->gh - 1 || !g->cells[r + 1][c]);
}
static bool has_lft(Galleta* g, int r, int c) {
    return g->v_edge[r][c] || (c == 0 || !g->cells[r][c - 1]);
}
static bool has_rgt(Galleta* g, int r, int c) {
    return g->v_edge[r][c + 1] || (c >= g->gw - 1 || !g->cells[r][c + 1]);
}

static int count_cell_edges(Galleta* g, int r, int c) {
    if(!g->cells[r][c]) return 0;
    return has_top(g, r, c) + has_bot(g, r, c) +
           has_lft(g, r, c) + has_rgt(g, r, c);
}

static bool is_h_border(Galleta* g, int r, int c) {
    if(c < 0 || c >= g->gw) return false;
    bool top = (r > 0 && r <= g->gh) ? g->cells[r - 1][c] : false;
    bool bot = (r >= 0 && r < g->gh) ? g->cells[r][c]     : false;
    return (top || bot) && (top != bot);
}

static bool is_v_border(Galleta* g, int r, int c) {
    if(r < 0 || r >= g->gh) return false;
    bool lft = (c > 0 && c <= g->gw) ? g->cells[r][c - 1] : false;
    bool rgt = (c >= 0 && c < g->gw) ? g->cells[r][c]     : false;
    return (lft || rgt) && (lft != rgt);
}

static bool is_placeable(Galleta* g, int fr, int fc) {
    if(fr < 0 || fc < 0) return false;
    bool is_h = (fr % 2 == 0) && (fc % 2 == 1);
    bool is_v = (fr % 2 == 1) && (fc % 2 == 0);

    if(is_h) {
        int r = fr / 2, c = fc / 2;
        if(r < 0 || r > g->gh || c < 0 || c >= g->gw) return false;
        if(g->h_edge[r][c]) return false;
        bool top = (r > 0)     ? g->cells[r - 1][c] : false;
        bool bot = (r < g->gh) ? g->cells[r][c]     : false;
        return top && bot;
    }
    if(is_v) {
        int r = fr / 2, c = fc / 2;
        if(r < 0 || r >= g->gh || c < 0 || c > g->gw) return false;
        if(g->v_edge[r][c]) return false;
        bool lft = (c > 0)     ? g->cells[r][c - 1] : false;
        bool rgt = (c < g->gw) ? g->cells[r][c]     : false;
        return lft && rgt;
    }
    return false;
}

static bool is_placeable_oriented(Galleta* g, int fr, int fc, bool want_h) {
    if(!is_placeable(g, fr, fc)) return false;
    bool is_h = (fr % 2 == 0) && (fc % 2 == 1);
    return want_h == is_h;
}

// ─── Cursor ───────────────────────────────────────────────────────────────────

static bool find_edge_oriented(Galleta* g, bool want_h) {
    int max = 2 * (g->gw + g->gh) + 2;
    for(int dist = 0; dist <= max; dist++) {
        for(int dr = -dist; dr <= dist; dr++) {
            int adc = dist - abs(dr);
            int dcs[2] = {-adc, adc};
            for(int d = 0; d < 2; d++) {
                int nr = g->cr + dr;
                int nc = g->cc + dcs[d];
                if(is_placeable_oriented(g, nr, nc, want_h)) {
                    g->cr = nr;
                    g->cc = nc;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool find_any_edge(Galleta* g) {
    if(find_edge_oriented(g, g->cur_h)) return true;
    g->cur_h = !g->cur_h;
    return find_edge_oriented(g, g->cur_h);
}

static void move_cursor(Galleta* g, int dr, int dc) {
    int mr = 2 * g->gh, mc = 2 * g->gw;
    int best_r = -1, best_c = -1;
    int best_dist = 9999;

    for(int fr = 0; fr <= mr; fr++) {
        for(int fc = 0; fc <= mc; fc++) {
            if(fr == g->cr && fc == g->cc) continue;
            if(!is_placeable_oriented(g, fr, fc, g->cur_h)) continue;

            int diff_r = fr - g->cr;
            int diff_c = fc - g->cc;

            // Must be in the requested direction
            if(dr == -1 && diff_r >= 0) continue;
            if(dr ==  1 && diff_r <= 0) continue;
            if(dc == -1 && diff_c >= 0) continue;
            if(dc ==  1 && diff_c <= 0) continue;

            // Primary axis distance + secondary penalty
            int primary = (dr != 0) ? abs(diff_r) : abs(diff_c);
            int secondary = (dr != 0) ? abs(diff_c) : abs(diff_r);
            int dist = primary * 3 + secondary;

            if(dist < best_dist) {
                best_dist = dist;
                best_r = fr;
                best_c = fc;
            }
        }
    }

    if(best_r >= 0) {
        g->cr = best_r;
        g->cc = best_c;
    }
}

static void rotate_cursor(Galleta* g) {
    bool new_h = !g->cur_h;
    int old_r = g->cr, old_c = g->cc;
    g->cur_h = new_h;
    if(find_edge_oriented(g, new_h)) return;
    // No edges in new orientation, revert
    g->cur_h = !new_h;
    g->cr = old_r;
    g->cc = old_c;
}

// ─── AI ───────────────────────────────────────────────────────────────────────

static int ai_would_complete(Galleta* g, int fr, int fc) {
    bool is_h = (fr % 2 == 0);
    int done = 0;
    if(is_h) g->h_edge[fr / 2][fc / 2] = true;
    else     g->v_edge[fr / 2][fc / 2] = true;

    if(is_h) {
        int r = fr / 2, c = fc / 2;
        if(r > 0     && g->cells[r-1][c] && g->owner[r-1][c] == 0 && count_cell_edges(g, r-1, c) == 4) done++;
        if(r < g->gh && g->cells[r][c]   && g->owner[r][c]   == 0 && count_cell_edges(g, r, c)   == 4) done++;
    } else {
        int r = fr / 2, c = fc / 2;
        if(c > 0     && g->cells[r][c-1] && g->owner[r][c-1] == 0 && count_cell_edges(g, r, c-1) == 4) done++;
        if(c < g->gw && g->cells[r][c]   && g->owner[r][c]   == 0 && count_cell_edges(g, r, c)   == 4) done++;
    }

    if(is_h) g->h_edge[fr / 2][fc / 2] = false;
    else     g->v_edge[fr / 2][fc / 2] = false;
    return done;
}

static int ai_would_gift(Galleta* g, int fr, int fc) {
    bool is_h = (fr % 2 == 0);
    int gifts = 0;
    if(is_h) g->h_edge[fr / 2][fc / 2] = true;
    else     g->v_edge[fr / 2][fc / 2] = true;

    if(is_h) {
        int r = fr / 2, c = fc / 2;
        if(r > 0     && g->cells[r-1][c] && g->owner[r-1][c] == 0 && count_cell_edges(g, r-1, c) == 3) gifts++;
        if(r < g->gh && g->cells[r][c]   && g->owner[r][c]   == 0 && count_cell_edges(g, r, c)   == 3) gifts++;
    } else {
        int r = fr / 2, c = fc / 2;
        if(c > 0     && g->cells[r][c-1] && g->owner[r][c-1] == 0 && count_cell_edges(g, r, c-1) == 3) gifts++;
        if(c < g->gw && g->cells[r][c]   && g->owner[r][c]   == 0 && count_cell_edges(g, r, c)   == 3) gifts++;
    }

    if(is_h) g->h_edge[fr / 2][fc / 2] = false;
    else     g->v_edge[fr / 2][fc / 2] = false;
    return gifts;
}

static void ai_choose_edge(Galleta* g) {
    int er[120], ec[120], ne = 0;
    for(int fr = 0; fr <= 2 * g->gh; fr++)
        for(int fc = 0; fc <= 2 * g->gw; fc++)
            if(is_placeable(g, fr, fc) && ne < 120) {
                er[ne] = fr;
                ec[ne] = fc;
                ne++;
            }
    if(ne == 0) return;

    // Easy: pure random
    if(g->difficulty == 0) {
        int pick = rand() % ne;
        g->cr = er[pick]; g->cc = ec[pick];
        return;
    }

    // Standard & Hard: always complete if possible (greedy)
    for(int i = 0; i < ne; i++) {
        if(ai_would_complete(g, er[i], ec[i]) > 0) {
            g->cr = er[i]; g->cc = ec[i];
            return;
        }
    }

    // Standard: random among remaining (doesn't care about gifting)
    if(g->difficulty == 1) {
        int pick = rand() % ne;
        g->cr = er[pick]; g->cc = ec[pick];
        return;
    }

    // Hard: avoid gifting 3-edge cells to opponent
    int safe[120], ns = 0;
    for(int i = 0; i < ne; i++)
        if(ai_would_gift(g, er[i], ec[i]) == 0)
            safe[ns++] = i;

    if(ns > 0) {
        int pick = safe[rand() % ns];
        g->cr = er[pick]; g->cc = ec[pick];
        return;
    }

    int best = 0, best_g = 99;
    for(int i = 0; i < ne; i++) {
        int gift = ai_would_gift(g, er[i], ec[i]);
        if(gift < best_g) { best_g = gift; best = i; }
    }
    g->cr = er[best]; g->cc = ec[best];
}

// ─── Shape generation ─────────────────────────────────────────────────────────

static void generate_shape(Galleta* g) {
    int size = rand() % 3;
    int dims_w[] = {5, 7, 9};
    int dims_h[] = {3, 4, 5};
    int css[]    = {14, 10, 8};

    g->gw = dims_w[size];
    g->gh = dims_h[size];
    g->cs = css[size];

    memset(g->cells, 0, sizeof(g->cells));
    for(int r = 0; r < g->gh; r++)
        for(int c = 0; c < g->gw; c++)
            g->cells[r][c] = true;

    int min_d = (g->gh < g->gw) ? g->gh : g->gw;
    int max_cut = min_d / 2;
    if(max_cut < 1) max_cut = 1;

    int d[4];
    bool any = false;
    for(int i = 0; i < 4; i++) {
        d[i] = rand() % (max_cut + 1);
        if(d[i] > 0) any = true;
    }
    if(!any) d[rand() % 4] = 1;

    for(int r = 0; r < g->gh; r++) {
        for(int c = 0; c < g->gw; c++) {
            if(r + c < d[0])                               g->cells[r][c] = false;
            if(r + (g->gw - 1 - c) < d[1])                g->cells[r][c] = false;
            if((g->gh - 1 - r) + c < d[2])                g->cells[r][c] = false;
            if((g->gh - 1 - r) + (g->gw - 1 - c) < d[3]) g->cells[r][c] = false;
        }
    }

    g->total = 0;
    for(int r = 0; r < g->gh; r++)
        for(int c = 0; c < g->gw; c++)
            if(g->cells[r][c]) g->total++;
}

// ─── Place edge & check completions ──────────────────────────────────────────

static void place_edge(Galleta* g) {
    bool is_h = (g->cr % 2 == 0);

    if(is_h) g->h_edge[g->cr / 2][g->cc / 2] = true;
    else     g->v_edge[g->cr / 2][g->cc / 2] = true;

    int adj_r[2], adj_c[2], nadj = 0;
    if(is_h) {
        int r = g->cr / 2, c = g->cc / 2;
        if(r > 0     && g->cells[r - 1][c]) { adj_r[nadj] = r - 1; adj_c[nadj] = c; nadj++; }
        if(r < g->gh && g->cells[r][c])     { adj_r[nadj] = r;     adj_c[nadj] = c; nadj++; }
    } else {
        int r = g->cr / 2, c = g->cc / 2;
        if(c > 0     && g->cells[r][c - 1]) { adj_r[nadj] = r; adj_c[nadj] = c - 1; nadj++; }
        if(c < g->gw && g->cells[r][c])     { adj_r[nadj] = r; adj_c[nadj] = c;     nadj++; }
    }

    int completed = 0;
    for(int i = 0; i < nadj; i++) {
        int r = adj_r[i], c = adj_c[i];
        if(g->owner[r][c] == 0 && count_cell_edges(g, r, c) == 4) {
            g->owner[r][c] = g->turn;
            completed++;
            g->claimed++;
            if(g->turn == 1) g->s1++; else g->s2++;
        }
    }

    if(g->claimed >= g->total) {
        g->scn = ScnResult;
        g->ai_pending = false;
        return;
    }

    if(completed > 0) {
        if(!g->two_player && g->turn == 2) {
            g->ai_pending = true;
            g->ai_timer   = furi_get_tick();
        }
        if(!find_any_edge(g)) g->scn = ScnResult;
    } else {
        g->turn = (g->turn == 1) ? 2 : 1;
        if(!g->two_player && g->turn == 2) {
            g->ai_pending = true;
            g->ai_timer   = furi_get_tick();
        }
        if(!find_any_edge(g)) g->scn = ScnResult;
    }
}

// ─── Start game ──────────────────────────────────────────────────────────────

static void start_game(Galleta* g) {
    generate_shape(g);
    memset(g->h_edge, 0, sizeof(g->h_edge));
    memset(g->v_edge, 0, sizeof(g->v_edge));
    memset(g->owner,  0, sizeof(g->owner));
    g->s1 = 0;
    g->s2 = 0;
    g->claimed    = 0;
    g->ai_pending = false;
    g->scn        = ScnGame;
    g->turn       = g->p1_starts ? 1 : 2;
    g->cur_h      = true;

    g->cr = 0;
    g->cc = 1;
    find_any_edge(g);

    if(!g->two_player && g->turn == 2) {
        g->ai_pending = true;
        g->ai_timer   = furi_get_tick();
    }
}

// ─── DRAW: idioma ─────────────────────────────────────────────────────────────

static void draw_lang(Canvas* canvas, Galleta* g) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 30, 12, "GALLETA");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 25, 27, "Idioma / Language");

    const char* opts[2] = {"  Espanol", "  English"};
    for(int i = 0; i < 2; i++) {
        int y = 42 + i * 14;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 20, y - 9, 88, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 24, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: reglas ─────────────────────────────────────────────────────────────

static void draw_rules(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 30, 10, L->title);
    canvas_draw_line(canvas, 0, 13, 128, 13);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 25, L->rules1);
    canvas_draw_str(canvas, 4, 37, L->rules2);
    canvas_draw_str(canvas, 4, 49, L->rules3);
    canvas_draw_line(canvas, 0, 55, 128, 55);
    canvas_draw_str(canvas, 25, 64, L->start);
}

// ─── DRAW: controles ─────────────────────────────────────────────────────────

static void draw_controls(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 25, 10, L->ctrl_title);
    canvas_draw_line(canvas, 0, 13, 128, 13);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 25, L->ctrl1);
    canvas_draw_str(canvas, 4, 35, L->ctrl2);
    canvas_draw_str(canvas, 4, 45, L->ctrl3);
    canvas_draw_str(canvas, 4, 55, L->ctrl4);
    canvas_draw_line(canvas, 0, 57, 128, 57);
    canvas_draw_str(canvas, 35, 64, L->ctrl_ok);
}

// ─── DRAW: modo ───────────────────────────────────────────────────────────────

static void draw_mode(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, L->mode_title);
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* opts[2] = {L->mode_1p, L->mode_2p};
    for(int i = 0; i < 2; i++) {
        int y = 34 + i * 16;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 10, y - 9, 108, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 14, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: dificultad ─────────────────────────────────────────────────────────

static void draw_diff(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 12, L->diff_title);
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* opts[3] = {L->diff_easy, L->diff_std, L->diff_hard};
    for(int i = 0; i < 3; i++) {
        int y = 30 + i * 13;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 10, y - 9, 108, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 14, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: quien empieza ─────────────────────────────────────────────────────

static void draw_first(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, L->who_first);
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* opts[2];
    if(g->two_player) {
        opts[0] = L->p1_first;
        opts[1] = L->p2_first;
    } else {
        opts[0] = L->me_first;
        opts[1] = L->flip_first;
    }
    for(int i = 0; i < 2; i++) {
        int y = 34 + i * 16;
        if(g->menu_sel == i) {
            canvas_draw_box(canvas, 10, y - 9, 108, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 14, y, opts[i]);
        if(g->menu_sel == i) canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 50, 64, "OK");
}

// ─── DRAW: juego ──────────────────────────────────────────────────────────────

static void draw_game(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);

    // Header (compact: 1px less)
    canvas_set_font(canvas, FontSecondary);
    char hdr[40];
    if(g->two_player)
        snprintf(hdr, sizeof(hdr), "J1:%d J2:%d", g->s1, g->s2);
    else
        snprintf(hdr, sizeof(hdr), "Tu:%d Flip:%d", g->s1, g->s2);
    canvas_draw_str(canvas, 2, 7, hdr);

    const char* trn;
    if(g->two_player)
        trn = (g->turn == 1) ? L->p1_turn : L->p2_turn;
    else
        trn = (g->turn == 1) ? L->your_turn : L->flip_turn;
    canvas_draw_str(canvas, 82, 7, trn);
    canvas_draw_line(canvas, 0, 8, 128, 8);

    int ox = (128 - g->gw * g->cs) / 2;
    int oy = 9 + (55 - g->gh * g->cs) / 2;
    int cs = g->cs;

    // Dots
    for(int r = 0; r <= g->gh; r++) {
        for(int c = 0; c <= g->gw; c++) {
            bool any = false;
            if(r > 0 && c > 0         && g->cells[r-1][c-1]) any = true;
            if(r > 0 && c < g->gw     && g->cells[r-1][c])   any = true;
            if(r < g->gh && c > 0     && g->cells[r][c-1])   any = true;
            if(r < g->gh && c < g->gw && g->cells[r][c])     any = true;
            if(any) {
                int px = ox + c * cs;
                int py = oy + r * cs;
                canvas_draw_box(canvas, px - 1, py - 1, 3, 3);
            }
        }
    }

    // Border edges
    for(int r = 0; r <= g->gh; r++)
        for(int c = 0; c < g->gw; c++)
            if(is_h_border(g, r, c))
                canvas_draw_line(canvas, ox + c*cs, oy + r*cs, ox + (c+1)*cs, oy + r*cs);

    for(int r = 0; r < g->gh; r++)
        for(int c = 0; c <= g->gw; c++)
            if(is_v_border(g, r, c))
                canvas_draw_line(canvas, ox + c*cs, oy + r*cs, ox + c*cs, oy + (r+1)*cs);

    // Placed edges
    for(int r = 0; r <= g->gh; r++)
        for(int c = 0; c < g->gw; c++)
            if(g->h_edge[r][c])
                canvas_draw_line(canvas, ox + c*cs, oy + r*cs, ox + (c+1)*cs, oy + r*cs);

    for(int r = 0; r < g->gh; r++)
        for(int c = 0; c <= g->gw; c++)
            if(g->v_edge[r][c])
                canvas_draw_line(canvas, ox + c*cs, oy + r*cs, ox + c*cs, oy + (r+1)*cs);

    // Hint dots
    for(int r = 0; r <= g->gh; r++)
        for(int c = 0; c < g->gw; c++) {
            if(!g->h_edge[r][c] && !is_h_border(g, r, c)) {
                bool top = (r > 0)     ? g->cells[r-1][c] : false;
                bool bot = (r < g->gh) ? g->cells[r][c]   : false;
                if(top && bot)
                    canvas_draw_dot(canvas, ox + c*cs + cs/2, oy + r*cs);
            }
        }

    for(int r = 0; r < g->gh; r++)
        for(int c = 0; c <= g->gw; c++) {
            if(!g->v_edge[r][c] && !is_v_border(g, r, c)) {
                bool lft = (c > 0)     ? g->cells[r][c-1] : false;
                bool rgt = (c < g->gw) ? g->cells[r][c]   : false;
                if(lft && rgt)
                    canvas_draw_dot(canvas, ox + c*cs, oy + r*cs + cs/2);
            }
        }

    // Cursor (thick line) — only shown when human can play
    if(g->two_player || g->turn == 1) {
        bool cur_is_h = (g->cr % 2 == 0);
        if(cur_is_h) {
            int r = g->cr / 2, c = g->cc / 2;
            int x1 = ox + c*cs + 1, x2 = ox + (c+1)*cs - 1;
            int y = oy + r * cs;
            canvas_draw_line(canvas, x1, y - 1, x2, y - 1);
            canvas_draw_line(canvas, x1, y,     x2, y);
            canvas_draw_line(canvas, x1, y + 1, x2, y + 1);
        } else {
            int r = g->cr / 2, c = g->cc / 2;
            int x = ox + c * cs;
            int y1 = oy + r*cs + 1, y2 = oy + (r+1)*cs - 1;
            canvas_draw_line(canvas, x - 1, y1, x - 1, y2);
            canvas_draw_line(canvas, x,     y1, x,     y2);
            canvas_draw_line(canvas, x + 1, y1, x + 1, y2);
        }
    }

    // Ownership marks
    for(int r = 0; r < g->gh; r++) {
        for(int c = 0; c < g->gw; c++) {
            if(g->owner[r][c] == 0) continue;
            int x1 = ox + c*cs + 2;
            int y1 = oy + r*cs + 2;
            int x2 = ox + (c+1)*cs - 2;
            int y2 = oy + (r+1)*cs - 2;
            if(g->owner[r][c] == 1) {
                canvas_draw_line(canvas, x1, y2, x2, y1);
            } else {
                canvas_draw_line(canvas, x1, y1, x2, y2);
                canvas_draw_line(canvas, x1, y2, x2, y1);
            }
        }
    }
}

// ─── DRAW: resultado ─────────────────────────────────────────────────────────

static void draw_result(Canvas* canvas, Galleta* g) {
    const Lang* L = g->L;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    const char* msg;
    if(g->two_player) {
        if(g->s1 > g->s2)      msg = L->p1_wins;
        else if(g->s2 > g->s1) msg = L->p2_wins;
        else                    msg = L->tie;
    } else {
        if(g->s1 > g->s2)      msg = L->you_win;
        else if(g->s2 > g->s1) msg = L->you_lose;
        else                    msg = L->tie;
    }
    canvas_draw_str(canvas, 10, 14, msg);
    canvas_draw_line(canvas, 0, 18, 128, 18);

    canvas_set_font(canvas, FontSecondary);
    char s1[32], s2[32];
    if(g->two_player) {
        snprintf(s1, sizeof(s1), "J1 /  : %d %s", g->s1, L->squares);
        snprintf(s2, sizeof(s2), "J2 X  : %d %s", g->s2, L->squares);
    } else {
        snprintf(s1, sizeof(s1), "Tu /  : %d %s", g->s1, L->squares);
        snprintf(s2, sizeof(s2), "Flip X: %d %s", g->s2, L->squares);
    }
    canvas_draw_str(canvas, 8, 32, s1);
    canvas_draw_str(canvas, 8, 44, s2);
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_draw_str(canvas, 4, 62, L->new_game);
}

// ─── Callbacks ───────────────────────────────────────────────────────────────

static void draw_cb(Canvas* canvas, void* ctx) {
    Galleta* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    switch(g->scn) {
    case ScnLang:     draw_lang(canvas, g);     break;
    case ScnRules:    draw_rules(canvas, g);    break;
    case ScnControls: draw_controls(canvas, g); break;
    case ScnMode:     draw_mode(canvas, g);     break;
    case ScnDiff:     draw_diff(canvas, g);     break;
    case ScnFirst:    draw_first(canvas, g);    break;
    case ScnGame:     draw_game(canvas, g);     break;
    case ScnResult:   draw_result(canvas, g);   break;
    }
    furi_mutex_release(g->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    Galleta* g = ctx;
    furi_message_queue_put(g->queue, event, 0);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int32_t galleta_app(void* p) {
    UNUSED(p);
    srand(furi_get_tick());

    Galleta* g = malloc(sizeof(Galleta));
    memset(g, 0, sizeof(Galleta));
    g->running    = true;
    g->scn        = ScnLang;
    g->L          = &LANG_ES;
    g->menu_sel   = 0;
    g->two_player = false;
    g->p1_starts  = true;
    g->cur_h      = true;

    g->mutex     = furi_mutex_alloc(FuriMutexTypeNormal);
    g->queue     = furi_message_queue_alloc(8, sizeof(InputEvent));
    g->view_port = view_port_alloc();
    view_port_draw_callback_set(g->view_port, draw_cb, g);
    view_port_input_callback_set(g->view_port, input_cb, g);
    g->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(g->gui, g->view_port, GuiLayerFullscreen);

    InputEvent ev;

    while(g->running) {
        if(furi_message_queue_get(g->queue, &ev, 100) == FuriStatusOk) {
            // Allow short and long presses
            if(ev.type != InputTypeShort && ev.type != InputTypeLong) continue;

            furi_mutex_acquire(g->mutex, FuriWaitForever);

            switch(g->scn) {
            case ScnLang:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                    if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                    if(ev.key == InputKeyOk) {
                        g->L        = (g->menu_sel == 0) ? &LANG_ES : &LANG_EN;
                        g->scn      = ScnRules;
                        g->menu_sel = 0;
                    }
                    if(ev.key == InputKeyBack) g->running = false;
                }
                break;

            case ScnRules:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyOk)   { g->scn = ScnControls; g->menu_sel = 0; }
                    if(ev.key == InputKeyBack) { g->scn = ScnLang;     g->menu_sel = 0; }
                }
                break;

            case ScnControls:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyOk)   { g->scn = ScnMode;  g->menu_sel = 0; }
                    if(ev.key == InputKeyBack) { g->scn = ScnRules; g->menu_sel = 0; }
                }
                break;

            case ScnMode:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                    if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                    if(ev.key == InputKeyOk) {
                        g->two_player = (g->menu_sel == 1);
                        g->scn        = g->two_player ? ScnFirst : ScnDiff;
                        g->menu_sel   = 0;
                    }
                    if(ev.key == InputKeyBack) { g->scn = ScnControls; g->menu_sel = 0; }
                }
                break;

            case ScnDiff:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                    if(ev.key == InputKeyDown && g->menu_sel < 2) g->menu_sel++;
                    if(ev.key == InputKeyOk) {
                        g->difficulty = g->menu_sel;
                        g->scn        = ScnFirst;
                        g->menu_sel   = 0;
                    }
                    if(ev.key == InputKeyBack) { g->scn = ScnMode; g->menu_sel = 0; }
                }
                break;

            case ScnFirst:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyUp   && g->menu_sel > 0) g->menu_sel--;
                    if(ev.key == InputKeyDown && g->menu_sel < 1) g->menu_sel++;
                    if(ev.key == InputKeyOk) {
                        g->p1_starts = (g->menu_sel == 0);
                        start_game(g);
                    }
                    if(ev.key == InputKeyBack) {
                        g->scn      = g->two_player ? ScnMode : ScnDiff;
                        g->menu_sel = 0;
                    }
                }
                break;

            case ScnGame:
                if(ev.type == InputTypeShort && ev.key == InputKeyBack) {
                    g->running = false;
                } else if(g->two_player || g->turn == 1) {
                    if(ev.type == InputTypeLong && ev.key == InputKeyOk) {
                        // Long press OK = rotate cursor
                        rotate_cursor(g);
                    } else if(ev.type == InputTypeShort) {
                        if(ev.key == InputKeyUp)    move_cursor(g, -1, 0);
                        if(ev.key == InputKeyDown)  move_cursor(g,  1, 0);
                        if(ev.key == InputKeyLeft)  move_cursor(g,  0, -1);
                        if(ev.key == InputKeyRight) move_cursor(g,  0,  1);
                        if(ev.key == InputKeyOk)    place_edge(g);
                    }
                }
                break;

            case ScnResult:
                if(ev.type == InputTypeShort) {
                    if(ev.key == InputKeyOk)   { g->scn = ScnFirst; g->menu_sel = 0; }
                    if(ev.key == InputKeyBack) { g->scn = ScnLang;  g->menu_sel = 0; }
                }
                break;
            }
            furi_mutex_release(g->mutex);
        }

        // AI turn
        furi_mutex_acquire(g->mutex, FuriWaitForever);
        if(g->scn == ScnGame &&
           !g->two_player &&
           g->turn == 2 &&
           g->ai_pending &&
           furi_get_tick() - g->ai_timer >= 800) {
            g->ai_pending = false;
            ai_choose_edge(g);
            place_edge(g);
        }
        furi_mutex_release(g->mutex);

        view_port_update(g->view_port);
    }

    gui_remove_view_port(g->gui, g->view_port);
    view_port_free(g->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(g->queue);
    furi_mutex_free(g->mutex);
    free(g);
    return 0;
}
