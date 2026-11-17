// ---------------------------------------------------------------------------
//  Face: "panel"  -  multi-panel digital LCD, mint backlight after dark
//
//  Modelled on a smartwatch face that mimics a segmented LCD split into a
//  dozen chamfered panels on a dark textured ground, each with its own
//  seven-segment readout, bold label and little icon, a tick scale with a red
//  marker across the top, a moon between the panels, and a yellow TIMEZONE
//  label at the foot. The original's health data is replaced with what this
//  clock actually knows:
//
//     sunrise time        temperature           day progress (the scale)
//     WiFi signal         month / weekday       moon phase        day-month
//     24H / PA / alarm    HH:MM  ss + weather icon
//     sunset time         daylight length
//                         GMT offset / TIMEZONE
//
//  Every readout is drawn as segments with the unlit ones faintly visible,
//  which is what makes it read as an LCD rather than printed digits. Panels
//  are grey-green by day and mint after sunset.
// ---------------------------------------------------------------------------
#include "../face.h"
#include <WiFi.h>
#include "../settings.h"
extern const uint8_t FontRuSmall[];

namespace face_panel {

// ---- palette ---------------------------------------------------------------
#define C_BG        0x0000
#define C_DOT       0x18C3   // background texture
#define C_FRAME     0x4A69   // thin outline around each panel
#define C_BAR       0x8410   // scale line, bezel text
#define C_TEXT      0xBDF7   // light text on the dark ground
#define C_YELLOW    0xFE60   // TIMEZONE
#define C_RED       0xF800   // scale marker
#define C_MOON      0xC618
#define C_MOON_DK   0x2104
#define C_PANEL_D   0xCE78   // day: grey-green LCD
#define C_GHOST_D   0xADB5   // unlit segments, a shade darker than the panel
#define C_PANEL_N   0x6F9A   // night: mint backlight
#define C_GHOST_N   0x5EF8
#define C_INK       0x0000

static uint16_t P_COL, G_COL;          // panel and ghost colour for this frame

uint16_t faceBackground() { return C_BG; }
bool     faceSmooth()     { return false; }
void     faceInit()       { }

static const uint8_t SEG_MAP[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// ---- primitives ------------------------------------------------------------
// A digit: bit a=0x01 b=0x02 c=0x04 d=0x08 e=0x10 f=0x20 g=0x40. d < 0 blank.
template <typename GFX>
static void seg7(GFX &g, int x, int y, int w, int h, int t, int d)
{
    uint8_t m    = (d >= 0 && d <= 9) ? SEG_MAP[d] : 0;
    int     half = (h - t) / 2;
    int     vh   = (h - 3 * t) / 2;
    if (vh < 1) vh = 1;
    struct S { uint8_t bit; int x, y, w, h; } s[7] = {
        {0x01, x + t,     y,            w - 2 * t, t },
        {0x20, x,         y + t,        t,         vh},
        {0x02, x + w - t, y + t,        t,         vh},
        {0x40, x + t,     y + half,     w - 2 * t, t },
        {0x10, x,         y + half + t, t,         vh},
        {0x04, x + w - t, y + half + t, t,         vh},
        {0x08, x + t,     y + h - t,    w - 2 * t, t },
    };
    for (int i = 0; i < 7; i++)
        g.fillRect(s[i].x, s[i].y, s[i].w, s[i].h, (m & s[i].bit) ? C_INK : G_COL);
}

// "HH:MM" or "d-m" style strings of digits, ':' and '-', in segments.
// Returns the width drawn. Characters: 0-9, ':' , '-', ' ' (blank digit).
// `colonCol` is what a ':' is drawn in. It defaults to the ink, because most
// of the readouts here are times of day - sunrise, sunset - whose separator
// has no business blinking. Only the clock passes something else, so its
// colon can tick on the second.
template <typename GFX>
static int segText(GFX &g, const char *s, int x, int y, int w, int h, int t, int gap,
                   int colonCol);

template <typename GFX>
static int segText(GFX &g, const char *s, int x, int y, int w, int h, int t, int gap)
{
    return segText(g, s, x, y, w, h, t, gap, (int)C_INK);
}

template <typename GFX>
static int segText(GFX &g, const char *s, int x, int y, int w, int h, int t, int gap,
                   int colonCol)
{
    int x0 = x;
    for (; *s; s++) {
        if (*s == ':') {
            int q = t;
            g.fillRect(x, y + h / 3 - q / 2, q, q, (uint16_t)colonCol);
            g.fillRect(x, y + 2 * h / 3 - q / 2, q, q, (uint16_t)colonCol);
            x += q + gap;
        } else if (*s == '-') {
            g.fillRect(x, y + (h - t) / 2, w - 2, t, C_INK);
            x += w - 2 + gap;
        } else {
            seg7(g, x, y, w, h, t, (*s >= '0' && *s <= '9') ? *s - '0' : -1);
            x += w + gap;
        }
    }
    return x - x0 - gap;
}

static int segWidth(const char *s, int w, int t, int gap)
{
    int x = 0;
    for (; *s; s++) x += ((*s == ':') ? t : (*s == '-') ? w - 2 : w) + gap;
    return x - gap;
}

// A panel with the corners cut, and a thin frame just outside it.
template <typename GFX>
static void panel(GFX &g, int x, int y, int w, int h)
{
    g.drawRect(x - 2, y - 2, w + 4, h + 4, C_FRAME);
    g.fillRect(x + 2, y, w - 4, h, P_COL);
    g.fillRect(x, y + 2, w, h - 4, P_COL);
    g.fillRect(x + 1, y + 1, w - 2, h - 2, P_COL);
}

template <typename GFX>
static void label(GFX &g, const char *s, int x, int y, uint8_t datum = TC_DATUM, uint8_t font = 2)
{
    g.setTextDatum(datum);
    g.setTextColor(C_INK, P_COL);
    g.drawString(s, x, y, font);
}

// ---- icons, all in ink on a panel ---------------------------------------
template <typename GFX>
static void sunHalf(GFX &g, int x, int y, bool rising)
{
    // half a sun on a horizon line, with an arrow up (rise) or down (set)
    g.fillCircle(x, y, 4, C_INK);
    g.fillRect(x - 6, y + 1, 13, 4, P_COL);
    g.drawFastHLine(x - 7, y + 2, 15, C_INK);
    g.drawLine(x - 6, y - 4, x - 4, y - 2, C_INK);
    g.drawLine(x + 6, y - 4, x + 4, y - 2, C_INK);
    g.drawFastVLine(x, y - 8, 3, C_INK);
    if (rising) { g.drawPixel(x - 1, y - 7, C_INK); g.drawPixel(x + 1, y - 7, C_INK); }
    else        { g.drawPixel(x - 1, y - 6, C_INK); g.drawPixel(x + 1, y - 6, C_INK); }
}

template <typename GFX>
static void thermometer(GFX &g, int x, int y)
{
    g.drawRect(x - 2, y - 8, 5, 11, C_INK);
    g.fillRect(x - 1, y - 3, 3, 6, C_INK);
    g.fillCircle(x, y + 5, 3, C_INK);
    g.drawFastHLine(x + 4, y - 6, 2, C_INK);
    g.drawFastHLine(x + 4, y - 3, 2, C_INK);
    g.drawFastHLine(x + 4, y, 2, C_INK);
}

template <typename GFX>
static void signalBars(GFX &g, int x, int y, int pct)
{
    // four bars, lit by strength, the rest ghosted
    for (int i = 0; i < 4; i++) {
        int h = 3 + i * 3;
        g.fillRect(x + i * 5, y - h, 4, h, pct > i * 25 ? C_INK : G_COL);
    }
}

// Знак процента 6x8 px, нарисованный линиями: не зависит от шрифта.
template <typename GFX>
static void pctGlyph(GFX &g, int x, int y, uint16_t c)
{
    g.drawRect(x,     y,     3, 3, c);   // верхний кружок
    g.drawRect(x + 3, y + 5, 3, 3, c);   // нижний кружок
    g.drawLine(x + 5, y, x, y + 7, c);   // косая черта
}

template <typename GFX>
static void bell(GFX &g, int x, int y, uint16_t c)
{
    g.fillTriangle(x - 5, y + 3, x + 5, y + 3, x, y - 6, c);
    g.fillRect(x - 6, y + 3, 13, 2, c);
    g.fillRect(x - 1, y + 5, 3, 2, c);
}

// Луна: освещённая часть заливается построчно строго внутри диска, поэтому
// за кольцо ничего не выходит. 0 = новолуние, 4 = полнолуние.
template <typename GFX>
static void moon(GFX &g, int cx, int cy, int r, int phase8)
{
    // cos(2*pi*p) * 100 для p = 0, 1/8 ... 7/8: положение терминатора
    static const int8_t K[8] = { 100, 71, 0, -71, -100, -71, 0, 71 };

    g.fillCircle(cx, cy, r + 3, C_MOON_DK);
    g.drawCircle(cx, cy, r + 3, C_FRAME);
    g.drawCircle(cx, cy, r, 0x3186);
    if (phase8 == 0) { g.fillCircle(cx, cy, r, 0x4208); return; }     // новолуние

    const bool waxing = phase8 <= 4;          // растущая: свет справа
    for (int dy = -r; dy <= r; dy++) {
        int hw = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
        if (hw <= 0) continue;
        int tx = hw * K[phase8] / 100;        // где проходит граница света
        int x0 = waxing ? cx + tx : cx - hw;
        int x1 = waxing ? cx + hw : cx - tx;
        if (x1 >= x0) g.drawFastHLine(x0, cy + dy, x1 - x0 + 1, C_MOON);
    }
}

// 0 = new, 4 = full. Days since a known new moon (2000-01-06 18:14 UTC),
// divided by the synodic month.
static int moonPhase8(time_t now)
{
    double days = (now - 947182440.0) / 86400.0;
    double p = days / 29.530588853;
    p -= (long)p;
    if (p < 0) p += 1;
    return ((int)(p * 8 + 0.5)) & 7;
}

static void hhmm(char *out, int minutes)
{
    if (minutes < 0) { strcpy(out, "--:--"); return; }
    snprintf(out, 8, "%02d:%02d", minutes / 60, minutes % 60);
}

// Название сохранённого города по центру. Если не помещается в maxW,
// обрезается по границе UTF-8 символа и получает ".." в конце.
// Вызывать, пока загружен русский шрифт: ширина считается по нему.
template <typename GFX>
static void drawLocation(GFX &g, int cx, int y, int maxW)
{
    String s = settings.locQuery.length() ? settings.locQuery : settings.locName;
    if (!s.length()) return;

    if (g.textWidth(s) > maxW) {
        while (s.length() && g.textWidth(s + "..") > maxW) {
            int i = s.length() - 1;
            while (i > 0 && (((uint8_t)s[i]) & 0xC0) == 0x80) i--;   // к началу символа
            s.remove(i);
        }
        s += "..";
    }
    g.setTextDatum(TC_DATUM);
    g.setTextColor(P_COL, C_BG);
    g.drawString(s, cx, y);
}

// ---- the face ---------------------------------------------------------------
template <typename GFX>
static void render(GFX &g, const struct tm &t, float)
{
    static const char *WD[7]   = { "ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ" };   // индекс = tm_wday
    static const char *MON[12] = { "ЯНВ", "ФЕВ", "МАР", "АПР", "МАЙ", "ИЮН",
                                   "ИЮЛ", "АВГ", "СЕН", "ОКТ", "НОЯ", "ДЕК" };

    bool night = isNightNow(t);
    P_COL = night ? C_PANEL_N : C_PANEL_D;
    G_COL = night ? C_GHOST_N : C_GHOST_D;

    g.fillScreen(C_BG);
    for (int y = 2; y < 240; y += 4)
        for (int x = (y & 4) ? 2 : 0; x < 240; x += 4)
            g.drawPixel(x, y, C_DOT);

    char buf[16];

    // ---- day-progress scale (встроенный шрифт 1, до загрузки русского) -----
    g.drawFastHLine(48, 77, 144, C_BAR);
    static const int PCT[5] = {0, 30, 50, 70, 100};
    static const char *PCTS[5] = {"0", "30", "50", "70", "100"};
    g.setTextDatum(TC_DATUM);
    g.setTextColor(C_TEXT, C_BG);
    for (int i = 0; i < 5; i++) {
        int x = 52 + PCT[i] * 136 / 100;
        g.drawFastVLine(x, 74, 7, C_BAR);
        g.drawString(PCTS[i], x, 65, 1);
    }
    if (wxSunrise >= 0 && wxSunset > wxSunrise) {
        int now = t.tm_hour * 60 + t.tm_min;
        int pct = (now - wxSunrise) * 100 / (wxSunset - wxSunrise);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        int x = 52 + pct * 136 / 100;
        g.fillTriangle(x - 4, 66, x + 4, 66, x, 73, C_RED);
    }

    g.loadFont(FontRuSmall);

    // ---- top row: sunrise | temperature ------------------------------------
    panel(g, 40, 30, 76, 32);
    hhmm(buf, wxSunrise);
    segText(g, buf, 40 + (76 - segWidth(buf, 8, 2, 2)) / 2 - 6, 33, 8, 12, 2, 2);
    label(g, "ВОСХОД", 70, 44);
    sunHalf(g, 106, 51, true);

    panel(g, 124, 30, 76, 32);
    if (wxValid) snprintf(buf, sizeof buf, "%d", wxTempF); else strcpy(buf, "--");
    {
        int w = segWidth(buf, 10, 2, 2);
        int x = 124 + (76 - w - 18) / 2;
        segText(g, buf, x, 33, 10, 14, 2, 2);
        g.setTextDatum(TL_DATUM);
        g.setTextColor(C_INK, P_COL);
        g.drawCircle(x + w + 4, 35, 2, C_INK);
        g.drawString(wxUnit(), x + w + 8, 32);
    }
    label(g, "ТЕМП", 152, 46);
    thermometer(g, 186, 52);

    // ---- second row: signal | weekday | moon | date ------------------------
    panel(g, 30, 84, 38, 34);
    int sig = wifiLevel();
    signalBars(g, 40, 97, sig);
    snprintf(buf, sizeof buf, "%d", sig);
    {
        const bool big = sig >= 100;
        const int  dw = big ? 7 : 8, dg = big ? 1 : 2, dh = 12, dt = 2;
        int w = segWidth(buf, dw, dt, dg);
        int x = 30 + (38 - (w + 3 + 6)) / 2;     // число + зазор 3 + знак 6
        segText(g, buf, x, 101, dw, dh, dt, dg);
        pctGlyph(g, x + w + 3, 103, C_INK);
    }

    panel(g, 72, 84, 40, 34);
    label(g, WD[t.tm_wday % 7], 92, 101, MC_DATUM);

    moon(g, 132, 101, 11, moonPhase8(time(nullptr)));

    // число месяца по центру ячейки (центр 179, как и у подписи месяца)
    panel(g, 150, 84, 58, 34);
    snprintf(buf, sizeof buf, "%02d", t.tm_mday);
    segText(g, buf, 150 + (58 - segWidth(buf, 10, 2, 3)) / 2, 87, 10, 17, 2, 3);
    label(g, MON[t.tm_mon % 12], 179, 104);

    // ---- the time ----------------------------------------------------------
    // Группа «часы + секунды» сдвинута влево на 19 px: теперь она занимает
    // x = 39..201 и стоит по центру экрана, как нижние ячейки (44..196).
    snprintf(buf, sizeof buf, "%02d:%02d", t.tm_hour, t.tm_min);
    panel(g, 39, 124, 130, 52);
    segText(g, buf, 43, 128, 24, 44, 5, 6,
            (t.tm_sec & 1) ? (int)C_INK : (int)P_COL);
    panel(g, 171, 124, 30, 52);
    snprintf(buf, sizeof buf, "%02d", t.tm_sec);
    segText(g, buf, 175, 154, 10, 18, 2, 3);
    wxIconMono(g, 186, 139, wxValid ? iconForCode(wxCode) : WX_UNKNOWN, !night, C_INK, P_COL);

    // ---- third row: sunset | daylight --------------------------------------
    panel(g, 44, 180, 74, 30);
    hhmm(buf, wxSunset);
    segText(g, buf, 44 + (74 - segWidth(buf, 8, 2, 2)) / 2 - 6, 183, 8, 12, 2, 2);
    label(g, "ЗАКАТ", 74, 194);
    sunHalf(g, 108, 200, false);

    panel(g, 122, 180, 74, 30);
    if (wxSunrise >= 0 && wxSunset > wxSunrise) hhmm(buf, wxSunset - wxSunrise);
    else strcpy(buf, "--:--");
    segText(g, buf, 122 + (74 - segWidth(buf, 8, 2, 2)) / 2, 183, 8, 12, 2, 2);
    label(g, "ДЕНЬ", 159, 194);

    // ---- foot: сохранённый город -------------------------------------------
    drawLocation(g, 120, 216, 100);

    g.unloadFont();
}

static void faceRender(TFT_eSprite &g, const struct tm &t, float sub) { render(g, t, sub); }
static void faceRender(TFT_eSPI    &g, const struct tm &t, float sub) { render(g, t, sub); }

} // namespace face_panel

const FaceVTable FACE_PANEL = {
    "panel",
    face_panel::faceInit,
    face_panel::faceBackground,
    face_panel::faceSmooth,
    (void(*)(TFT_eSprite&, const struct tm&, float))face_panel::faceRender,
    (void(*)(TFT_eSPI&,    const struct tm&, float))face_panel::faceRender,
};
