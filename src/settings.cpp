#include "settings.h"
#include <Preferences.h>

Settings settings;
static const char *NS = "clock";

void settingsLoad()
{
    Preferences p;
    p.begin(NS, true);
    settings.ssid      = p.getString("ssid", "");
    settings.pass      = p.getString("pass", "");
    settings.locQuery  = p.getString("loc", "Barnaul");
    settings.locName   = p.getString("locname", "Barnaul");
    settings.tzName    = p.getString("tz", "Asia/Barnaul");
    settings.lat       = p.getFloat("lat", 53.3548f);
    settings.lon       = p.getFloat("lon", 83.7698f);
    settings.hasLoc    = p.getBool("hasloc", true);
    settings.useF      = p.getBool("useF", false);
    settings.utcOffset = p.getInt("utcoff",25200);
    settings.hasOffset = p.getBool("hasoff", true);
    p.end();
}

void settingsSave()
{
    Preferences p;
    p.begin(NS, false);
    p.putString("ssid",    settings.ssid);
    p.putString("pass",    settings.pass);
    p.putString("loc",     settings.locQuery);
    p.putString("locname", settings.locName);
    p.putString("tz",      settings.tzName);
    p.putFloat ("lat",     settings.lat);
    p.putFloat ("lon",     settings.lon);
    p.putBool  ("hasloc",  settings.hasLoc);
    p.putBool  ("useF",    settings.useF);
    p.putInt   ("utcoff",  settings.utcOffset);
    p.putBool  ("hasoff",  settings.hasOffset);
    p.end();
}

void settingsClear()
{
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
    settings = Settings();
}
