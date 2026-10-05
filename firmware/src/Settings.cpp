#include "Settings.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static std::string cached;

void Settings::begin()
{
    prefs.begin("charmcfg", false);
    String s = prefs.getString("name", DEFAULT_NAME);
    cached = s.c_str();
    if (cached.empty())
        cached = DEFAULT_NAME;
}

std::string Settings::name()
{
    return cached;
}

bool Settings::setName(const std::string &utf8)
{
    std::string s;
    for (char c : utf8)
        if (c != '\r' && c != '\n' && c != '\t')
            s.push_back(c);
    size_t a = s.find_first_not_of(' ');
    size_t b = s.find_last_not_of(' ');
    s = a == std::string::npos ? "" : s.substr(a, b - a + 1);
    if (s.empty() || s.size() > NAME_MAX_BYTES)
        return false;
    cached = s;
    prefs.putString("name", s.c_str());
    return true;
}
