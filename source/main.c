// Monsterspiel - Region 1 Prototyp (Textmodus). Ungetestet: bitte Fehler melden.
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "data.h"
#define iprintf printf

#include <fat.h>
typedef struct { short id; int lv, exp, hp, maxhp, st[6]; short mv[4]; int nmv; int status, sleep; int stage[6]; int conf, flinch; } Pm;
enum { S_NONE, S_BRN, S_PAR, S_PSN, S_SLP, S_FRZ, S_CONF, S_FLINCH };
static const char *SN[8] = {"", "BRN", "PAR", "PSN", "SLP", "FRZ", "VRW", ""};
static Pm party[6]; static int np = 0, box = 0, money = 500, balls = 10, potions = 5;
static int px = 3, py = 3;

#define MW 30
#define MH 18
static const char *MAP[MH] = {
"##############################",
"#............wwwwww..........#",
"#..H.........wwwwww....wwww..#",
"#............wwwwww....wwww..#",
"#..........................w.#",
"#....####......##..........w.#",
"#....#..#......##....wwww....#",
"#....####..........wwwwwww...#",
"#..................wwwwwww...#",
"#..wwwww...........wwwwww....#",
"#..wwwww..........#####......#",
"#..wwwww..........#...#......#",
"#.................#...#..wwww#",
"#.....wwwwww............wwww.#",
"#.....wwwwww..........H..wwww#",
"#.....wwwwww.................#",
"#............................#",
"##############################"};

static int rnd(int n) { return rand() % n; }
static void wait_a(void) { while (1) { swiWaitForVBlank(); scanKeys(); if (keysDown() & (KEY_A | KEY_START)) return; } }
static int key(void) { while (1) { swiWaitForVBlank(); scanKeys(); int k = keysDown(); if (k) return k; } }
static void cls(void) { iprintf("\x1b[2J"); }

static void calc(Pm *p) {
    const Mon *m = &MON[p->id]; int old = p->maxhp;
    for (int i = 0; i < 6; i++) {
        int b = m->base[i];
        p->st[i] = (i == 0) ? (b * 2 * p->lv / 100 + p->lv + 10) : (b * 2 * p->lv / 100 + 5);
    }
    p->maxhp = p->st[0]; p->hp += p->maxhp - old; if (p->hp > p->maxhp) p->hp = p->maxhp;
}
static void relearn(Pm *p, int announce) {
    const Mon *m = &MON[p->id];
    for (int i = 0; i < m->l_n; i++) {
        const Learn *l = &LEARN[m->l_start + i];
        if (l->lv > p->lv) continue;
        int has = 0; for (int j = 0; j < p->nmv; j++) if (p->mv[j] == l->mv) has = 1;
        if (has) continue;
        if (p->nmv < 4) p->mv[p->nmv++] = l->mv;
        else { if (l->lv < p->lv - 6 && !announce) continue; for (int j = 0; j < 3; j++) p->mv[j] = p->mv[j + 1]; p->mv[3] = l->mv; }
        if (announce && l->lv == p->lv) iprintf("%s lernt %s!\n", m->name, MOVES[l->mv].name);
    }
}
static Pm make(int id, int lv) {
    Pm p; memset(&p, 0, sizeof p); p.id = id; p.lv = lv; calc(&p); p.hp = p.maxhp;
    relearn(&p, 0);
    if (p.nmv == 0) { p.mv[0] = 0; p.nmv = 1; }
    return p;
}
static int need(int lv) { return lv * lv * lv / 2 + 10; }


static int stagemul(int base, int s) { return s >= 0 ? base * (2 + s) / 2 : base * 2 / (2 - s); }
static int estat(const Pm *p, int k) { int v = stagemul(p->st[k], p->stage[k - 1]); if (k == 5 && p->status == S_PAR) v /= 2; return v < 1 ? 1 : v; }
static void reset_vol(Pm *p) { for (int i = 0; i < 6; i++) p->stage[i] = 0; p->conf = 0; p->flinch = 0; }
static const char *STN_[6] = {"Angriff", "Vert.", "Sp.-Ang.", "Sp.-Vert.", "Init.", "Genauigk."};
static void setstatus(Pm *t, int s, const Mon *m) {
    if (s == S_FLINCH) { t->flinch = 1; return; }
    if (s == S_CONF) { if (!t->conf) { t->conf = 2 + rnd(3); iprintf("%s ist verwirrt!\n", m->name); } return; }
    if (t->status != S_NONE || t->hp <= 0) return;
    if (s == S_BRN && (m->t1 == 1 || m->t2 == 1)) return;
    if (s == S_PSN && (m->t1 == 7 || m->t2 == 7 || m->t1 == 16 || m->t2 == 16)) return;
    if (s == S_FRZ && (m->t1 == 5 || m->t2 == 5)) return;
    if (s == S_PAR && (m->t1 == 4 || m->t2 == 4)) return;
    t->status = s; if (s == S_SLP) t->sleep = 1 + rnd(3);
    static const char *txt[8] = {"", "verbrennt", "ist gelaehmt", "wird vergiftet", "schlaeft ein", "gefriert", "", ""};
    iprintf("%s %s!\n", m->name, txt[s]);
}
static void stagechg(Pm *t, int i, int d) {
    int n = t->stage[i] + d; if (n > 6) n = 6; if (n < -6) n = -6;
    if (n == t->stage[i]) { iprintf("%s %s geht nicht weiter.\n", MON[t->id].name, STN_[i]); return; }
    t->stage[i] = n; iprintf("%s %s %s%d.\n", MON[t->id].name, STN_[i], d > 0 ? "+" : "", d);
}
static int eff(int mt, const Mon *d) {
    int e = CHART[mt][d->t1]; int r = e;
    int a = e, b = d->t2 == 255 ? 2 : CHART[mt][d->t2];
    // multiply: 0->0,1->.5,2->1,3->2 in quarters (x4)
    static const int q[4] = {0, 2, 4, 8};
    r = q[a] * q[b] / 4; return r; // 4 = normal
}
static int damage(const Pm *a, const Pm *d, int mi, int *effv) {
    const Move *mv = &MOVES[mi]; const Mon *am = &MON[a->id], *dm = &MON[d->id];
    int at = mv->cat ? estat(a, 3) : estat(a, 1), df = mv->cat ? estat(d, 4) : estat(d, 2);
    if (!mv->cat && a->status == S_BRN) at /= 2;
    int dmg = ((2 * a->lv / 5 + 2) * mv->power * at / (df ? df : 1)) / 50 + 2;
    if (mv->type == am->t1 || mv->type == am->t2) dmg = dmg * 3 / 2;
    int e = eff(mv->type, dm); *effv = e; dmg = dmg * e / 4;
    dmg = dmg * (85 + rnd(16)) / 100; if (e > 0 && dmg < 1) dmg = 1; return dmg;
}
static void msg_eff(int e) { if (e == 0) iprintf("Keine Wirkung...\n"); else if (e > 4) iprintf("Sehr effektiv!\n"); else if (e < 4) iprintf("Nicht sehr effektiv.\n"); }


static int fat_ok = 0;
typedef struct { unsigned magic; int np, box, money, balls, potions, px, py; Pm party[6]; } Save;
static int save_game(void) {
    if (!fat_ok) return 0;
    Save sv; memset(&sv, 0, sizeof sv); sv.magic = 0x4D4F4E31; sv.np = np; sv.box = box; sv.money = money; sv.balls = balls; sv.potions = potions; sv.px = px; sv.py = py;
    memcpy(sv.party, party, sizeof party);
    FILE *f = fopen("monsterspiel.sav", "wb"); if (!f) return 0;
    int ok = fwrite(&sv, sizeof sv, 1, f) == 1; fclose(f); return ok;
}
static int load_game(void) {
    if (!fat_ok) return 0;
    FILE *f = fopen("monsterspiel.sav", "rb"); if (!f) return 0;
    static Save sv; int ok = fread(&sv, sizeof sv, 1, f) == 1; fclose(f);
    if (!ok || sv.magic != 0x4D4F4E31 || sv.np < 1 || sv.np > 6) return 0;
    np = sv.np; box = sv.box; money = sv.money; balls = sv.balls; potions = sv.potions; px = sv.px; py = sv.py;
    memcpy(party, sv.party, sizeof party);
    for (int i = 0; i < np; i++) if (party[i].id < 0 || party[i].id >= NMON) return 0;
    return 1;
}
static int first_alive(void) { for (int i = 0; i < np; i++) if (party[i].hp > 0) return i; return -1; }
static void heal_all(void) { for (int i = 0; i < np; i++) { party[i].hp = party[i].maxhp; party[i].status = 0; reset_vol(&party[i]); } }

static void gain(Pm *p, int xp) {
    p->exp += xp; iprintf("%s erhaelt %d EP.\n", MON[p->id].name, xp);
    while (p->exp >= need(p->lv) && p->lv < 100) {
        p->exp -= need(p->lv); p->lv++; calc(p); iprintf("%s ist jetzt Lv %d!\n", MON[p->id].name, p->lv);
        relearn(p, 1);
        const Mon *m = &MON[p->id];
        if (m->evo_to >= 0 && p->lv >= m->evo_lv) { iprintf("%s entwickelt sich zu %s!\n", m->name, MON[m->evo_to].name); p->id = m->evo_to; calc(&*p); relearn(p, 1); }
    }
}


static int attack(Pm *a, Pm *d, int mi, int mine) {
    const Move *mv = &MOVES[mi]; const Mon *am = &MON[a->id];
    const char *who = mine ? "" : "Gegner ";
    // Vor-Checks
    if (a->flinch) { a->flinch = 0; iprintf("%s%s zuckt zurueck!\n", who, am->name); return 0; }
    if (a->status == S_SLP) { if (--a->sleep <= 0) { a->status = 0; iprintf("%s%s wacht auf!\n", who, am->name); } else { iprintf("%s%s schlaeft.\n", who, am->name); return 0; } }
    if (a->status == S_FRZ) { if (rnd(100) < 20) { a->status = 0; iprintf("%s%s taut auf!\n", who, am->name); } else { iprintf("%s%s ist eingefroren.\n", who, am->name); return 0; } }
    if (a->status == S_PAR && rnd(4) == 0) { iprintf("%s%s ist gelaehmt!\n", who, am->name); return 0; }
    if (a->conf > 0) { a->conf--; iprintf("%s%s ist verwirrt.\n", who, am->name);
        if (rnd(3) == 0) { int dm = (2 * a->lv / 5 + 2) * 40 * estat(a, 1) / estat(a, 2) / 50 + 2; a->hp -= dm; if (a->hp < 0) a->hp = 0; iprintf("Es verletzt sich selbst!\n"); return 0; } }
    iprintf("%s%s setzt %s ein!\n", who, am->name, mv->name);
    if (!mv->always && mv->acc < 100) {
        int acc = stagemul(mv->acc, a->stage[5]);
        if (rnd(100) >= acc) { iprintf("Daneben!\n"); return 0; }
    }
    int total = 0;
    if (mv->power > 0) {
        int hits = mv->hmin + (mv->hmax > mv->hmin ? rnd(mv->hmax - mv->hmin + 1) : 0);
        int e = 4;
        for (int h = 0; h < hits && d->hp > 0; h++) {
            int dm = damage(a, d, mi, &e);
            if (mv->crit && rnd(100) < (mv->crit == 2 ? 100 : 12)) { dm = dm * 3 / 2; iprintf("Volltreffer!\n"); }
            d->hp -= dm; if (d->hp < 0) d->hp = 0; total += dm;
        }
        if (hits > 1) iprintf("%d Treffer!\n", hits);
        msg_eff(e);
        if (mv->drain && total > 0) { a->hp += total * mv->drain / 100; if (a->hp > a->maxhp) a->hp = a->maxhp; iprintf("%s saugt KP ab.\n", am->name); }
        if (mv->recoil && total > 0) { int r = total / mv->recoil; if (r < 1) r = 1; a->hp -= r; if (a->hp < 0) a->hp = 0; iprintf("%s erleidet Rueckstoss.\n", am->name); }
    }
    if (mv->power == 0 && mv->heal) { a->hp += a->maxhp * mv->heal / 100; if (a->hp > a->maxhp) a->hp = a->maxhp; iprintf("%s heilt sich.\n", am->name); }
    // Zusatzeffekte (nur wenn Ziel noch steht oder Selbst-Effekt)
    if (rnd(100) < mv->schance || mv->schance == 100) {
        Pm *tg = mv->sself ? a : d;
        if (mv->status && (mv->sself || d->hp > 0)) setstatus(mv->sself ? a : d, mv->status, &MON[tg->id]);
        for (int i = 0; i < 6; i++) if (mv->dl[i] && (mv->sself || d->hp > 0 || mv->power == 0)) stagechg(tg, i, mv->dl[i]);
    }
    return total;
}
static void end_turn(Pm *p, int mine) {
    if (p->hp <= 0) return;
    if (p->status == S_BRN) { p->hp -= p->maxhp / 16 + 1; iprintf("%s%s leidet unter Verbrennung.\n", mine ? "" : "Gegner ", MON[p->id].name); }
    else if (p->status == S_PSN) { p->hp -= p->maxhp / 8 + 1; iprintf("%s%s leidet unter Gift.\n", mine ? "" : "Gegner ", MON[p->id].name); }
    if (p->hp < 0) p->hp = 0;
}

// returns 1 if won/caught, 0 otherwise
static void battle(int wid, int wlv) {
    Pm w = make(wid, wlv);
    for (int i = 0; i < np; i++) reset_vol(&party[i]);
    int cur = first_alive(); if (cur < 0) return;
    cls(); iprintf("Ein wildes %s (Lv %d) erscheint!\n", MON[w.id].name, w.lv); wait_a();
    while (1) {
        Pm *p = &party[cur];
        cls();
        iprintf("%s Lv%d %s/%s %s\n  HP %d/%d\n", MON[w.id].name, w.lv, TYPE_NAMES[MON[w.id].t1], MON[w.id].t2 == 255 ? "-" : TYPE_NAMES[MON[w.id].t2], SN[w.status], w.hp, w.maxhp);
        iprintf("%s Lv%d %s\n  HP %d/%d\n\n", MON[p->id].name, p->lv, SN[p->status], p->hp, p->maxhp);
        iprintf("A:Kampf B:Flucht\nX:Ball(%d) Y:Trank(%d)\n", balls, potions);
        int k = key(), act = 0, mi = -1;
        if (k & KEY_A) {
            cls(); iprintf("Welche Attacke?\n");
            for (int i = 0; i < p->nmv; i++) iprintf(" %d) %s [%s] %s%d\n", i + 1, MOVES[p->mv[i]].name, TYPE_NAMES[MOVES[p->mv[i]].type], MOVES[p->mv[i]].power ? "St" : "-", MOVES[p->mv[i]].power);
            iprintf("\nA/B/X/Y = 1..4, START=zurueck\n");
            int kk = key();
            int sel = (kk & KEY_A) ? 0 : (kk & KEY_B) ? 1 : (kk & KEY_X) ? 2 : (kk & KEY_Y) ? 3 : -1;
            if (sel < 0 || sel >= p->nmv) continue;
            mi = p->mv[sel]; act = 1;
        } else if (k & KEY_B) {
            if (rnd(100) < 60 + (estat(p, 5) - estat(&w, 5))) { cls(); iprintf("Du bist entkommen.\n"); wait_a(); return; }
            cls(); iprintf("Flucht misslungen!\n"); act = 2;
        } else if (k & KEY_X) {
            if (balls <= 0) continue;
            balls--; cls(); iprintf("Du wirfst eine Kugel...\n");
            int chance = 30 + (w.maxhp - w.hp) * 60 / w.maxhp; if (w.status == S_SLP || w.status == S_FRZ) chance += 25; else if (w.status) chance += 10; if (MON[w.id].legend) chance /= 4;
            if (rnd(100) < chance) {
                iprintf("%s wurde gefangen!\n", MON[w.id].name);
                if (np < 6) party[np++] = w; else { box++; iprintf("Ab in die Box (%d).\n", box); }
                wait_a(); return;
            }
            iprintf("Es ist ausgebrochen!\n"); act = 2;
        } else if (k & KEY_Y) {
            if (potions <= 0 || p->hp == p->maxhp) continue;
            potions--; p->status = 0; p->hp += 40; if (p->hp > p->maxhp) p->hp = p->maxhp;
            cls(); iprintf("%s wurde geheilt.\n", MON[p->id].name); act = 2;
        } else continue;
        // Reihenfolge (Prioritaet, dann Initiative)
        int wmv = w.mv[0]; { int best = -1; for (int i = 0; i < w.nmv; i++) { int sc = MOVES[w.mv[i]].power; if (sc > best) { best = sc; wmv = w.mv[i]; } } if (rnd(3) == 0) wmv = w.mv[rnd(w.nmv)]; }
        int pp = (act == 1) ? MOVES[mi].prio : 0, wp = MOVES[wmv].prio;
        int pfirst;
        if (act != 1) pfirst = 0; else if (pp != wp) pfirst = pp > wp; else pfirst = estat(p, 5) >= estat(&w, 5);
        if (act == 1 && pfirst) {
            attack(p, &w, mi, 1);
            if (w.hp > 0 && p->hp > 0) attack(&w, p, wmv, 0);
        } else if (act == 1) {
            attack(&w, p, wmv, 0);
            if (p->hp > 0 && w.hp > 0) attack(p, &w, mi, 1);
        } else attack(&w, p, wmv, 0);
        end_turn(p, 1); end_turn(&w, 0); w.flinch = 0; p->flinch = 0;
        if (w.hp <= 0) {
            int xp = w.lv * (MON[w.id].base[0] + MON[w.id].base[1]) / 12 + 5; if (MON[w.id].legend) xp *= 3;
            iprintf("%s wurde besiegt!\n", MON[w.id].name); gain(&party[cur], xp); money += w.lv * 8; wait_a(); return;
        }
        if (p->hp <= 0) {
            iprintf("%s ist besiegt!\n", MON[p->id].name);
            int n = first_alive();
            if (n < 0) { iprintf("Du eilst zum Heiler...\n"); money /= 2; heal_all(); px = 3; py = 3; wait_a(); return; }
            cur = n; iprintf("Los, %s!\n", MON[party[cur].id].name);
        }
        wait_a();
    }
}

static void draw(void) {
    cls();
    for (int y = 0; y < MH; y++) {
        char line[MW + 1];
        for (int x = 0; x < MW; x++) line[x] = (x == px && y == py) ? '@' : MAP[y][x];
        line[MW] = 0; iprintf("%s\n", line);
    }
    iprintf("%s Lv%d HP %d/%d\n", MON[party[0].id].name, party[0].lv, party[0].hp, party[0].maxhp);
    iprintf("Taler %d Ball %d Trank %d\nSELECT=Team START=Speichern", money, balls, potions);
}
static void team(void) {
    cls(); iprintf("TEAM (Box: %d)\n\n", box);
    for (int i = 0; i < np; i++) iprintf("%d %s Lv%d %d/%d %s\n", i + 1, MON[party[i].id].name, party[i].lv, party[i].hp, party[i].maxhp, TYPE_NAMES[MON[party[i].id].t1]);
    iprintf("\nA = zurueck\n"); wait_a();
}
static void shop(void) {
    heal_all(); cls(); save_game(); iprintf("HEILERIN & LADEN\nTeam geheilt, gespeichert.\nA: Ball kaufen (100)\nB: Trank kaufen (50)\nSTART: weiter\nTaler %d\n", money); 
    while (1) { int k = key(); if (k & KEY_START) return;
        if ((k & KEY_A) && money >= 100) { money -= 100; balls++; } if ((k & KEY_B) && money >= 50) { money -= 50; potions++; }
        iprintf("Taler %d Ball %d Trank %d   \n", money, balls, potions); }
}

int main(void) {
    consoleDemoInit(); keysSetRepeat(15, 4); srand(time(NULL)); fat_ok = fatInitDefault();
    cls(); iprintf("\n\n   MONSTERSPIEL\n   Region 1 - Test\n\n   Druecke START\n"); wait_a();
    int loaded = 0;
    if (load_game()) { cls(); iprintf("Spielstand gefunden.\nA: Weiter  B: Neu\n"); while (1) { int k = key(); if (k & KEY_A) { loaded = 1; break; } if (k & KEY_B) { np = 0; break; } } }
    else if (!fat_ok) { cls(); iprintf("Kein Speicher (SD) gefunden.\nSpeichern ist aus.\n"); wait_a(); }
    if (!loaded) {
    cls(); iprintf("Waehle dein Starter-Monster:\n\n");
    for (int i = 0; i < 3; i++) iprintf(" %s %s (%s)\n", i == 0 ? "A:" : i == 1 ? "B:" : "X:", MON[STARTERS[i]].name, TYPE_NAMES[MON[STARTERS[i]].t1]);
    int s = -1; while (s < 0) { int k = key(); s = (k & KEY_A) ? 0 : (k & KEY_B) ? 1 : (k & KEY_X) ? 2 : -1; }
    party[np++] = make(STARTERS[s], 5);
    }
    while (1) {
        draw(); swiWaitForVBlank(); scanKeys(); int k = keysDown() | (keysDownRepeat());
        int nx = px, ny = py;
        if (k & KEY_UP) ny--; else if (k & KEY_DOWN) ny++; else if (k & KEY_LEFT) nx--; else if (k & KEY_RIGHT) nx++;
        else if (k & KEY_SELECT) { team(); continue; }
        else if (k & KEY_START) { cls(); iprintf(save_game() ? "Gespeichert.\n" : "Speichern nicht moeglich.\n"); wait_a(); continue; }
        else continue;
        char c = MAP[ny][nx]; if (c == '#') continue; px = nx; py = ny;
        if (c == 'H') { shop(); continue; }
        if (c == 'w' && rnd(100) < 14) {
            int lead = 0; for (int i = 0; i < np; i++) if (party[i].lv > lead) lead = party[i].lv;
            int lv = lead - 2 + rnd(4); if (lv < 2) lv = 2;
            int id = rnd(100) < 3 ? 150 : WILD[rnd(sizeof(WILD) / sizeof(WILD[0]))];
            battle(id, lv);
        }
    }
    return 0;
}
