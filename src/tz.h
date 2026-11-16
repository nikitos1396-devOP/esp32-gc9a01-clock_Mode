// Timezone: IANA zone name from the geocoder -> POSIX rule string.
// Russia has no DST, so every zone here is a fixed offset. Anything not in
// the table falls back to the UTC offset reported by the weather feed.
#pragma once
#include <Arduino.h>
#include <time.h>
#include "settings.h"

struct TzRule { const char *iana; const char *posix; };

static const TzRule TZ_TABLE[] = {
    {"Europe/Kaliningrad",    "EET-2"},
    {"Europe/Moscow",         "MSK-3"},
    {"Europe/Simferopol",     "MSK-3"},
    {"Europe/Volgograd",      "MSK-3"},
    {"Europe/Kirov",          "MSK-3"},
    {"Europe/Samara",         "<+04>-4"},
    {"Europe/Saratov",        "<+04>-4"},
    {"Europe/Astrakhan",      "<+04>-4"},
    {"Europe/Ulyanovsk",      "<+04>-4"},
    {"Asia/Yekaterinburg",    "<+05>-5"},
    {"Asia/Omsk",             "<+06>-6"},
    {"Asia/Novosibirsk",      "<+07>-7"},
    {"Asia/Barnaul",          "<+07>-7"},
    {"Asia/Tomsk",            "<+07>-7"},
    {"Asia/Novokuznetsk",     "<+07>-7"},
    {"Asia/Krasnoyarsk",      "<+07>-7"},
    {"Asia/Irkutsk",          "<+08>-8"},
    {"Asia/Chita",            "<+09>-9"},
    {"Asia/Yakutsk",          "<+09>-9"},
    {"Asia/Vladivostok",      "<+10>-10"},
    {"Asia/Sakhalin",         "<+11>-11"},
    {"Asia/Magadan",          "<+11>-11"},
    {"Asia/Kamchatka",        "<+12>-12"},
    {"UTC",                   "UTC0"},
};

inline const char *posixForZone(const String &iana)
{
    for (const TzRule &r : TZ_TABLE)
        if (iana == r.iana) return r.posix;
    return nullptr;
}

inline const char *tzString(const char *override)
{
    static char buf[32];
    if (override && override[0]) return override;
    if (const char *p = posixForZone(settings.tzName)) return p;
    if (settings.hasOffset) {
        int o = -settings.utcOffset;               // POSIX counts west as positive
        int h = abs(o) / 3600, m = (abs(o) % 3600) / 60;
        snprintf(buf, sizeof buf, "LOC%s%d:%02d", o < 0 ? "-" : "", h, m);
        return buf;
    }
    return "UTC0";
}

inline void applyTimezone(const char *override)
{
    setenv("TZ", tzString(override), 1);
    tzset();
}
