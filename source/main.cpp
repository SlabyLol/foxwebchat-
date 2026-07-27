#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <cctype>
#include <utility>
#include <algorithm>
#include <vector>
#include <string>
#include <curl/curl.h>

#define FIREBASE_URL "https://foxwebchat-bd592-default-rtdb.europe-west1.firebasedatabase.app"
#define ADMIN_CODE "AdminJs93€=no"
#define TECHNOBLADE_JOIN_TEXT "The Legend is here.... TECHNOBLADE JOINED"
#define UPDATE_CIA_URL "https://github.com/SlabyLol/foxwebchat-/releases/download/nightly/FoxWebChat.cia"
#define UPDATE_CIA_DIR "sdmc:/3ds/FoxWebChat"
#define UPDATE_CIA_PATH "sdmc:/3ds/FoxWebChat/FoxWebChat.cia"
#define THEME_CFG_PATH "sdmc:/3ds/FoxWebChat/theme.cfg"
#define CUSTOM_THEMES_DIR "sdmc:/3ds/FoxWebChat/themes"

// ---------------------------------------------------------------------
// Themes: multiple color schemes, switchable with D-Pad Left/Right.
// Custom themes can be dropped as .fwct files into CUSTOM_THEMES_DIR.
// ---------------------------------------------------------------------
struct Theme {
    std::string name;
    u32 bg;        // Main accent color (header bar, bottom screen)
    u32 mid;       // Darker accent shade
    u32 white;     // Fox / bright areas
    u32 cream;     // Secondary light text
    u32 dark;      // Main text (top screen)
    u32 selectBg;  // Highlight for the selected row
    u32 admin;     // Admin/warning color
    u32 muted;     // Muted hint text
    u32 textColor; // Text on the bottom screen (Web Edition, Controls, etc.)
};

static const Theme THEMES[] = {
    { "Orange",       C2D_Color32(247,127,51,255), C2D_Color32(225,90,35,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,247,240,255),
                       C2D_Color32(61,33,26,255),   C2D_Color32(255,213,181,255),
                       C2D_Color32(214,40,40,255),  C2D_Color32(120,90,80,255),
                       C2D_Color32(61,33,26,255) },

    { "Blau/Violett",  C2D_Color32(88,80,220,255),  C2D_Color32(58,50,168,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(238,236,255,255),
                       C2D_Color32(28,24,58,255),   C2D_Color32(205,200,255,255),
                       C2D_Color32(230,60,60,255),  C2D_Color32(150,145,195,255),
                       C2D_Color32(28,24,58,255) },

    { "Feuerrot",      C2D_Color32(210,58,40,255),  C2D_Color32(165,38,26,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,233,228,255),
                       C2D_Color32(48,20,16,255),   C2D_Color32(255,190,175,255),
                       C2D_Color32(255,205,0,255),  C2D_Color32(150,90,80,255),
                       C2D_Color32(48,20,16,255) },

    { "Dunkel",        C2D_Color32(38,38,44,255),   C2D_Color32(22,22,26,255),
                       C2D_Color32(235,235,240,255), C2D_Color32(200,200,210,255),
                       C2D_Color32(235,235,240,255), C2D_Color32(95,95,115,255),
                       C2D_Color32(255,90,90,255),  C2D_Color32(150,150,162,255),
                       C2D_Color32(235,235,240,255) },

    { "Wald",          C2D_Color32(60,140,70,255),  C2D_Color32(38,105,50,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(235,250,236,255),
                       C2D_Color32(24,45,28,255),   C2D_Color32(195,235,200,255),
                       C2D_Color32(214,40,40,255),  C2D_Color32(120,155,125,255),
                       C2D_Color32(24,45,28,255) },

    { "Pastell",       C2D_Color32(240,150,190,255), C2D_Color32(215,110,155,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,240,247,255),
                       C2D_Color32(70,30,48,255),   C2D_Color32(255,215,232,255),
                       C2D_Color32(214,40,40,255),  C2D_Color32(180,120,150,255),
                       C2D_Color32(70,30,48,255) },

    { "Sonnenschein",  C2D_Color32(250,190,40,255), C2D_Color32(220,155,10,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,248,225,255),
                       C2D_Color32(70,52,10,255),   C2D_Color32(255,232,170,255),
                       C2D_Color32(214,40,40,255),  C2D_Color32(150,130,80,255),
                       C2D_Color32(70,52,10,255) },

    { "Tuerkis",       C2D_Color32(35,170,165,255), C2D_Color32(20,125,120,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(228,252,250,255),
                       C2D_Color32(15,55,53,255),   C2D_Color32(180,240,236,255),
                       C2D_Color32(230,60,60,255),  C2D_Color32(110,160,158,255),
                       C2D_Color32(15,55,53,255) },
};
static const int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);
static std::vector<Theme> allThemes;  // built-in themes + loaded .fwct files
static int currentThemeIndex = 0;
static const Theme* currentTheme = &THEMES[0];  // repointed to &allThemes[...] in main()

// Hidden bonus theme - not part of THEMES[], only added to allThemes once unlocked
// via the SELECT+L secret combo (see main()).
static const Theme SECRET_THEME = {
    "\u2728 Secret Fox \u2728",
    C2D_Color32(212,175,55,255),  C2D_Color32(30,20,10,255),
    C2D_Color32(255,255,255,255), C2D_Color32(255,248,225,255),
    C2D_Color32(40,30,10,255),   C2D_Color32(255,223,120,255),
    C2D_Color32(214,40,40,255),  C2D_Color32(150,120,60,255),
    C2D_Color32(40,30,10,255)
};
#define SECRET_FLAG_PATH "sdmc:/3ds/FoxWebChat/.secret_unlocked"
static bool secretThemeUnlocked = false;

// ---------------------------------------------------------------------
// Color palette - always reads from the currently selected theme
// ---------------------------------------------------------------------
#define C_BG        (currentTheme->bg)
#define C_MID       (currentTheme->mid)
#define C_WHITE     (currentTheme->white)
#define C_CREAM     (currentTheme->cream)
#define C_DARK      (currentTheme->dark)
#define C_SELECT_BG (currentTheme->selectBg)
#define C_ADMIN     (currentTheme->admin)
#define C_MUTED     (currentTheme->muted)
#define C_TEXT      (currentTheme->textColor)
#define C_BLACK     C2D_Color32(0,0,0,255)

struct ChatMessage {
    std::string user;
    std::string text;
};

struct ReportItem {
    std::string user;
    std::string text;
    std::string reason;
};

static char username[64] = "";
static bool isAdmin = false;
static bool isKicked = false;
static bool showAdminPanel = false;
static char kickReason[128] = "";

std::vector<ChatMessage> messageList;
std::vector<ReportItem> reportList;
int selectedMsgIndex = 0;
int selectedReportIndex = 0;
u64 lastFetchTime = 0;

// Auto-scroll: while true, the selection automatically jumps to the newest
// message whenever new ones arrive (like a normal chat). Gets disabled as soon
// as the user manually scrolls up, and re-enabled once they reach the bottom again.
static bool followLatestMsg = true;

// ---------------------------------------------------------------------
// citro2d render targets & text buffer
// ---------------------------------------------------------------------
static C3D_RenderTarget* topTarget;
static C3D_RenderTarget* bottomTarget;
static C2D_TextBuf dynamicBuf;

// Progress display for downloads (update CIA, theme files). Defined further down,
// only forward-declared here since the download functions already need it.
static void draw_progress_screen(const std::string& label, float progress);

static float g_downloadProgress = 0.0f;   // 0.0 .. 1.0
static std::string g_downloadLabel = "Downloading...";

// Called regularly by libcurl during a download; updates the progress value
// and draws a new frame right away, so the bar keeps moving even during the
// (blocking) curl_easy_perform() call.
static int xfer_progress_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                   curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        g_downloadProgress = (float)dlnow / (float)dltotal;
    }

    C2D_TextBufClear(dynamicBuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        draw_progress_screen(g_downloadLabel, g_downloadProgress);
    C3D_FrameEnd(0);

    return 0; // non-zero would abort the download
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string firebase_get(const char* path) {
    CURL *curl = curl_easy_init();
    std::string readBuffer = "";
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

void firebase_post(const char* path, const char* json_data) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);

        struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void firebase_put(const char* path, const char* json_data) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);

        struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void firebase_delete(const char* path) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

static void ensure_dir(const char* path) {
    if (mkdir(path, 0777) != 0 && errno != EEXIST) {
        // ignore - if this genuinely fails, the later fopen() will fail anyway
    }
}

// Parses a single "r,g,b(,a)" line from a .fwct file
static bool parse_fwct_color(const std::string& value, u32& outColor) {
    int r = 0, g = 0, b = 0, a = 255;
    int n = sscanf(value.c_str(), "%d,%d,%d,%d", &r, &g, &b, &a);
    if (n == 3 || n == 4) {
        r = std::max(0, std::min(255, r));
        g = std::max(0, std::min(255, g));
        b = std::max(0, std::min(255, b));
        a = std::max(0, std::min(255, a));
        outColor = C2D_Color32(r, g, b, a);
        return true;
    }
    return false;
}

// Loads a single .fwct file. Missing lines keep the value from "base" (fallback: Orange theme).
static bool load_fwct_file(const std::string& path, const Theme& base, Theme& outTheme) {
    FILE* fp = fopen(path.c_str(), "r");
    if (!fp) return false;

    Theme t = base;
    bool gotAny = false;
    char lineBuf[256];

    while (fgets(lineBuf, sizeof(lineBuf), fp)) {
        std::string line(lineBuf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        u32 col;
        if (key == "name")           { t.name = val; gotAny = true; }
        else if (key == "bg"         && parse_fwct_color(val, col)) { t.bg = col; gotAny = true; }
        else if (key == "mid"        && parse_fwct_color(val, col)) { t.mid = col; gotAny = true; }
        else if (key == "white"      && parse_fwct_color(val, col)) { t.white = col; gotAny = true; }
        else if (key == "cream"      && parse_fwct_color(val, col)) { t.cream = col; gotAny = true; }
        else if (key == "dark"       && parse_fwct_color(val, col)) { t.dark = col; gotAny = true; }
        else if (key == "selectBg"   && parse_fwct_color(val, col)) { t.selectBg = col; gotAny = true; }
        else if (key == "muted"      && parse_fwct_color(val, col)) { t.muted = col; gotAny = true; }
        else if (key == "textColor"  && parse_fwct_color(val, col)) { t.textColor = col; gotAny = true; }
    }
    fclose(fp);

    if (!gotAny) return false;
    outTheme = t;
    return true;
}

// Creates the theme directory and, on the very first launch, writes an example file
// so it's immediately clear what a custom .fwct theme should look like.
static void ensure_custom_themes_dir() {
    ensure_dir("sdmc:/3ds");
    ensure_dir(UPDATE_CIA_DIR);

    struct stat st;
    bool existed = (stat(CUSTOM_THEMES_DIR, &st) == 0);
    ensure_dir(CUSTOM_THEMES_DIR);

    if (!existed) {
        std::string examplePath = std::string(CUSTOM_THEMES_DIR) + "/example.fwct";
        FILE* fp = fopen(examplePath.c_str(), "w");
        if (fp) {
            fprintf(fp,
                "# FoxWebChat theme file (.fwct)\n"
                "# Colors as R,G,B,A (0-255, A optional, defaults to 255). Lines starting with # are comments.\n"
                "# Copy this file, rename it, and tweak the values to make your own theme.\n"
                "name=My Theme\n"
                "bg=247,127,51\n"
                "mid=225,90,35\n"
                "white=255,255,255\n"
                "cream=255,247,240\n"
                "dark=61,33,26\n"
                "selectBg=255,213,181\n"
                "muted=120,90,80\n"
                "textColor=61,33,26\n"
            );
            fclose(fp);
        }
    }
}

// Scans CUSTOM_THEMES_DIR for .fwct files and appends valid themes to allThemes.
static void load_custom_themes() {
    ensure_custom_themes_dir();

    DIR* d = opendir(CUSTOM_THEMES_DIR);
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        std::string fname = entry->d_name;
        if (fname.size() <= 5) continue;
        if (fname.substr(fname.size() - 5) != ".fwct") continue;

        Theme t = THEMES[0]; // Orange as the base for any missing values
        t.name = fname.substr(0, fname.size() - 5);

        std::string fullPath = std::string(CUSTOM_THEMES_DIR) + "/" + fname;
        Theme loaded;
        if (load_fwct_file(fullPath, t, loaded)) {
            allThemes.push_back(loaded);
        }
    }
    closedir(d);
}

// Builds the full theme list (built-in + custom). Call before load_theme().
// Checks whether the secret theme has been unlocked before (marker file on SD card)
#define SECRET_FLAG_MAGIC "FWCT-SECRET-9f2c"

static void load_secret_flag() {
    FILE* fp = fopen(SECRET_FLAG_PATH, "r");
    if (fp) {
        char buf[32] = {0};
        fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        if (strcmp(buf, SECRET_FLAG_MAGIC) == 0) {
            secretThemeUnlocked = true;
        }
    }
}

static void unlock_secret_theme() {
    if (secretThemeUnlocked) return;
    secretThemeUnlocked = true;
    ensure_dir("sdmc:/3ds");
    ensure_dir(UPDATE_CIA_DIR);
    FILE* fp = fopen(SECRET_FLAG_PATH, "w");
    if (fp) {
        fprintf(fp, "%s", SECRET_FLAG_MAGIC);
        fclose(fp);
    }
}

static void init_themes() {
    allThemes.clear();
    for (int i = 0; i < THEME_COUNT; i++) allThemes.push_back(THEMES[i]);
    load_custom_themes();
    if (secretThemeUnlocked) allThemes.push_back(SECRET_THEME);
}

// Save / load the theme selection on the SD card
static void load_theme() {
    FILE* fp = fopen(THEME_CFG_PATH, "r");
    if (fp) {
        int idx = -1;
        if (fscanf(fp, "%d", &idx) == 1 && idx >= 0 && idx < (int)allThemes.size()) {
            currentThemeIndex = idx;
        }
        fclose(fp);
    }
    if (currentThemeIndex >= (int)allThemes.size()) currentThemeIndex = 0;
    currentTheme = &allThemes[currentThemeIndex];
}

static void save_theme() {
    ensure_dir("sdmc:/3ds");
    ensure_dir(UPDATE_CIA_DIR);
    FILE* fp = fopen(THEME_CFG_PATH, "w");
    if (fp) {
        fprintf(fp, "%d", currentThemeIndex);
        fclose(fp);
    }
}

// Downloads the current .cia from the GitHub release page and saves it
// to /3ds/FoxWebChat/FoxWebChat.cia on the SD card.
static bool download_update_cia(std::string& statusOut) {
    ensure_dir("sdmc:/3ds");
    ensure_dir(UPDATE_CIA_DIR);

    FILE* fp = fopen(UPDATE_CIA_PATH, "wb");
    if (!fp) {
        statusOut = "Could not open file for writing";
        return false;
    }

    g_downloadProgress = 0.0f;
    g_downloadLabel = "Downloading update...";

    CURL* curl = curl_easy_init();
    bool ok = false;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, UPDATE_CIA_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_progress_cb);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (res == CURLE_OK && httpCode == 200) {
            ok = true;
        } else if (res != CURLE_OK) {
            statusOut = std::string("curl error: ") + curl_easy_strerror(res);
        } else {
            statusOut = "HTTP " + std::to_string(httpCode);
        }

        curl_easy_cleanup(curl);
    } else {
        statusOut = "curl init failed";
    }

    fclose(fp);

    if (!ok) {
        remove(UPDATE_CIA_PATH);
    }
    return ok;
}

// ---------------------------------------------------------------------
// Themes from GitHub: checks the repo's themes/ folder and offers to
// download any new .fwct files.
// ---------------------------------------------------------------------
#define GITHUB_THEMES_API_URL "https://api.github.com/repos/SlabyLol/foxwebchat-/contents/themes"

struct RemoteTheme {
    std::string name;
    std::string downloadUrl;
};
static std::vector<RemoteTheme> pendingNewThemes;

std::string github_get(const std::string& url) {
    CURL *curl = curl_easy_init();
    std::string readBuffer = "";
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "FoxWebChat-3DS");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

// Finds all "name"/"download_url" pairs in a GitHub contents JSON, in order.
// Deliberately not a real JSON parser (nested "_links" objects would break
// brace-based parsing) - GitHub always emits "name" before "download_url".
static std::vector<std::pair<std::string, std::string>> parse_name_download_pairs(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> result;
    size_t pos = 0;

    while (true) {
        size_t namePos = json.find("\"name\"", pos);
        if (namePos == std::string::npos) break;

        size_t colon = json.find(":", namePos);
        size_t q1 = json.find("\"", colon);
        size_t q2 = (q1 == std::string::npos) ? std::string::npos : json.find("\"", q1 + 1);
        if (colon == std::string::npos || q1 == std::string::npos || q2 == std::string::npos) break;
        std::string name = json.substr(q1 + 1, q2 - q1 - 1);

        size_t dlPos = json.find("\"download_url\"", q2);
        if (dlPos == std::string::npos) break;
        size_t colon2 = json.find(":", dlPos);
        if (colon2 == std::string::npos) break;

        size_t searchStart = colon2 + 1;
        while (searchStart < json.size() && isspace((unsigned char)json[searchStart])) searchStart++;

        std::string downloadUrl;
        if (json.compare(searchStart, 4, "null") != 0) {
            size_t dq1 = json.find("\"", colon2);
            size_t dq2 = (dq1 == std::string::npos) ? std::string::npos : json.find("\"", dq1 + 1);
            if (dq1 != std::string::npos && dq2 != std::string::npos) {
                downloadUrl = json.substr(dq1 + 1, dq2 - dq1 - 1);
            }
        }

        result.push_back({name, downloadUrl});
        pos = dlPos + 1;
    }

    return result;
}

static std::vector<std::string> list_local_theme_files() {
    std::vector<std::string> files;
    DIR* d = opendir(CUSTOM_THEMES_DIR);
    if (!d) return files;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        std::string fname = entry->d_name;
        if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".fwct") {
            files.push_back(fname);
        }
    }
    closedir(d);
    return files;
}

// Compares the themes in the GitHub repo against the locally present .fwct files
// and fills pendingNewThemes with anything still missing.
void check_for_new_themes() {
    pendingNewThemes.clear();
    ensure_custom_themes_dir();

    std::string json = github_get(GITHUB_THEMES_API_URL);
    if (json.empty()) return;

    auto pairs = parse_name_download_pairs(json);
    auto localFiles = list_local_theme_files();

    for (auto& p : pairs) {
        const std::string& name = p.first;
        const std::string& url = p.second;

        if (name.size() <= 5 || name.substr(name.size() - 5) != ".fwct") continue;
        if (url.empty()) continue;

        bool existsLocally = false;
        for (auto& lf : localFiles) {
            if (lf == name) { existsLocally = true; break; }
        }
        if (!existsLocally) {
            pendingNewThemes.push_back({name, url});
        }
    }
}

// Downloads a single theme file found via GitHub into CUSTOM_THEMES_DIR.
static bool download_theme_file(const RemoteTheme& theme, std::string& statusOut) {
    ensure_custom_themes_dir();
    std::string outPath = std::string(CUSTOM_THEMES_DIR) + "/" + theme.name;

    FILE* fp = fopen(outPath.c_str(), "wb");
    if (!fp) {
        statusOut = "Could not open file for writing";
        return false;
    }

    CURL* curl = curl_easy_init();
    bool ok = false;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, theme.downloadUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "FoxWebChat-3DS");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_progress_cb);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (res == CURLE_OK && httpCode == 200) {
            ok = true;
        } else if (res != CURLE_OK) {
            statusOut = std::string("curl error: ") + curl_easy_strerror(res);
        } else {
            statusOut = "HTTP " + std::to_string(httpCode);
        }

        curl_easy_cleanup(curl);
    } else {
        statusOut = "curl init failed";
    }

    fclose(fp);

    if (!ok) {
        remove(outPath.c_str());
    }
    return ok;
}

void check_kick_status() {
    if(strlen(username) == 0) return;
    char path[128];
    snprintf(path, sizeof(path), "kicks/%s", username);
    std::string json = firebase_get(path);

    if(!json.empty() && json != "null") {
        isKicked = true;
        snprintf(kickReason, sizeof(kickReason), "Kicked by Admin");
    } else {
        isKicked = false;
    }
}

std::string parse_json_value(const std::string& block, const std::string& key) {
    size_t keyPos = block.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "Unknown";

    size_t colonPos = block.find(":", keyPos);
    if (colonPos == std::string::npos) return "Unknown";

    size_t startQuote = block.find("\"", colonPos);
    if (startQuote == std::string::npos) return "Unknown";

    size_t endQuote = block.find("\"", startQuote + 1);
    if (endQuote == std::string::npos) return "Unknown";

    return block.substr(startQuote + 1, endQuote - startQuote - 1);
}

void fetch_messages() {
    int previousCount = (int)messageList.size();

    std::string json = firebase_get("messages");
    messageList.clear();

    if(!(json.empty() || json == "null")) {
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t endPos = json.find("}", pos);
            if (endPos == std::string::npos) break;

            std::string block = json.substr(pos, endPos - pos + 1);
            std::string text = parse_json_value(block, "text");
            std::string user = parse_json_value(block, "user");

            if (text != "Unknown") {
                messageList.push_back({user, text});
            }
            pos = endPos + 1;
        }
    }

    // Auto-scroll: as long as the user is "following" the latest message,
    // automatically jump to whichever message is newest.
    if (followLatestMsg || selectedMsgIndex >= previousCount - 1) {
        selectedMsgIndex = messageList.empty() ? 0 : (int)messageList.size() - 1;
        followLatestMsg = true;
    }

    if (selectedMsgIndex >= (int)messageList.size()) {
        selectedMsgIndex = messageList.empty() ? 0 : (int)messageList.size() - 1;
    }
    if (selectedMsgIndex < 0) selectedMsgIndex = 0;
}

void fetch_reports() {
    std::string json = firebase_get("reports");
    reportList.clear();

    if(!(json.empty() || json == "null")) {
        size_t pos = 0;
        while ((pos = json.find("{", pos)) != std::string::npos) {
            size_t endPos = json.find("}", pos);
            if (endPos == std::string::npos) break;

            std::string block = json.substr(pos, endPos - pos + 1);
            std::string user = parse_json_value(block, "reportedUser");
            std::string text = parse_json_value(block, "messageText");
            std::string reason = parse_json_value(block, "reason");

            if (user != "Unknown") {
                reportList.push_back({user, text, reason});
            }
            pos = endPos + 1;
        }
    }

    if (selectedReportIndex >= (int)reportList.size()) {
        selectedReportIndex = reportList.empty() ? 0 : (int)reportList.size() - 1;
    }
    if (selectedReportIndex < 0) selectedReportIndex = 0;
}

#ifndef APP_VERSION_STR
#define APP_VERSION_STR "1.0.0"
#endif

static std::string updateStatus = "Checking for updates...";
static bool updateAvailable = false;

static std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

// Expects a simple string value under the "version" path in Firebase,
// e.g. set via the Firebase console: version: "1.0.1"
void check_for_updates() {
    updateStatus = "Checking for updates...";
    std::string json = firebase_get("version");

    if (json.empty() || json == "null") {
        updateStatus = "Update check failed";
        updateAvailable = false;
        return;
    }

    std::string latestVersion = strip_quotes(json);
    if (latestVersion.empty()) {
        updateStatus = "Update check failed";
        updateAvailable = false;
        return;
    }

    if (latestVersion != APP_VERSION_STR) {
        updateAvailable = true;
        updateStatus = "Update available: v" + latestVersion;
    } else {
        updateAvailable = false;
        updateStatus = "Up to date (v" APP_VERSION_STR ")";
    }
}

void open_keyboard(char* out_text, size_t max_len, const char* hint) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hint);
    swkbdInputText(&swkbd, out_text, max_len);
}

// ---------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------

static std::string truncate_text(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 3) + "...";
}

static void draw_text(float x, float y, float scale, u32 color, const std::string& str) {
    C2D_Text text;
    C2D_TextParse(&text, dynamicBuf, str.c_str());
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

static void draw_text_centered(float centerX, float y, float scale, u32 color, const std::string& str) {
    C2D_Text text;
    C2D_TextParse(&text, dynamicBuf, str.c_str());
    C2D_TextOptimize(&text);
    float w, h;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    C2D_DrawText(&text, C2D_WithColor, centerX - w/2.0f, y, 0.5f, scale, scale, color);
}

// Fox head in a flat style (matching the icon & banner), drawn purely with vectors
static void draw_fox(float cx, float cy, float w) {
    // Ears (white outside)
    C2D_DrawTriangle(cx-w*0.46f, cy-w*0.62f, C_WHITE,
                      cx-w*0.62f, cy-w*0.14f, C_WHITE,
                      cx-w*0.08f, cy-w*0.30f, C_WHITE, 0.5f);
    C2D_DrawTriangle(cx+w*0.46f, cy-w*0.62f, C_WHITE,
                      cx+w*0.62f, cy-w*0.14f, C_WHITE,
                      cx+w*0.08f, cy-w*0.30f, C_WHITE, 0.5f);
    // Inner ears (accent color)
    C2D_DrawTriangle(cx-w*0.40f, cy-w*0.46f, C_MID,
                      cx-w*0.49f, cy-w*0.22f, C_MID,
                      cx-w*0.20f, cy-w*0.30f, C_MID, 0.5f);
    C2D_DrawTriangle(cx+w*0.40f, cy-w*0.46f, C_MID,
                      cx+w*0.49f, cy-w*0.22f, C_MID,
                      cx+w*0.20f, cy-w*0.30f, C_MID, 0.5f);
    // Head
    C2D_DrawEllipseSolid(cx - w*0.72f/2, cy - w*0.02f - w*0.62f/2, 0.5f, w*0.72f, w*0.62f, C_WHITE);
    // Muzzle
    C2D_DrawTriangle(cx-w*0.14f, cy+w*0.02f, C_BG,
                      cx+w*0.14f, cy+w*0.02f, C_BG,
                      cx,         cy+w*0.30f, C_BG, 0.5f);
    // Eyes
    C2D_DrawCircleSolid(cx-w*0.16f, cy-w*0.02f, 0.5f, w*0.045f, C_DARK);
    C2D_DrawCircleSolid(cx+w*0.16f, cy-w*0.02f, 0.5f, w*0.045f, C_DARK);
    // Nose
    C2D_DrawTriangle(cx-w*0.045f, cy+w*0.14f, C_DARK,
                      cx+w*0.045f, cy+w*0.14f, C_DARK,
                      cx,          cy+w*0.14f+w*0.045f*1.3f, C_DARK, 0.5f);
}

// ---------------------------------------------------------------------
// Top screen: title, status lines, message/report list
// ---------------------------------------------------------------------
static void draw_top_screen() {
    const float SCREEN_W = 400.0f;
    const float HEADER_H = 34.0f;
    const float LIST_BOTTOM = 230.0f;
    const int ROW_H = 18;

    // Header bar
    C2D_DrawRectSolid(0, 0, 0.5f, SCREEN_W, HEADER_H, C_BG);
    draw_text(8, 8, 0.62f, C_WHITE, "FoxWebChat");

    std::string statusLine = strlen(username) > 0 ? std::string(username) : "Not joined";
    if (isAdmin) statusLine += "  (ADMIN)";
    draw_text(150, 12, 0.48f, isAdmin ? C2D_Color32(255,225,120,255) : C_WHITE, statusLine);

    if (isKicked) {
        draw_text(20, 90, 0.62f, C_ADMIN, "You have been kicked by an Admin!");
        draw_text(20, 118, 0.52f, C_DARK, std::string("Reason: ") + kickReason);
        draw_text(20, 155, 0.48f, C_MUTED, "Press [START] to exit");
        return;
    }

    if (strlen(username) == 0) {
        draw_text(20, 60, 0.58f, C_DARK, "Press (A) to enter name / join");
        draw_text(20, 88, 0.48f, C_MUTED, "Press [START] to exit");
        return;
    }

    float y = HEADER_H + 8;

    if (showAdminPanel) {
        draw_text(8, y, 0.55f, C_ADMIN, "=== ADMIN PANEL ==="); y += 22;
        draw_text(8, y, 0.44f, C_MUTED, "(B) Close  (X) Kick  (Y) Clear Reports"); y += 16;
        draw_text(8, y, 0.44f, C_MUTED, "(L) Clear Messages  (R) Unban All"); y += 20;

        if (reportList.empty()) {
            draw_text(8, y, 0.48f, C_MUTED, "No active reports.");
        } else {
            int total = (int)reportList.size();
            int maxVisible = std::max(1, (int)((LIST_BOTTOM - y) / ROW_H));
            int scrollOffset = 0;
            if (total > maxVisible) {
                scrollOffset = selectedReportIndex - maxVisible + 1;
                if (scrollOffset < 0) scrollOffset = 0;
                int maxOffset = total - maxVisible;
                if (scrollOffset > maxOffset) scrollOffset = maxOffset;

                char counter[32];
                snprintf(counter, sizeof(counter), "%d-%d/%d", scrollOffset + 1, std::min(total, scrollOffset + maxVisible), total);
                draw_text(SCREEN_W - 70, y - 16, 0.32f, C_MUTED, counter);
            }
            int endIdx = std::min(total, scrollOffset + maxVisible);

            for (int i = scrollOffset; i < endIdx; i++) {
                bool sel = i == selectedReportIndex;
                if (sel) C2D_DrawRectSolid(4, y-2, 0.4f, SCREEN_W-8, 18, C_SELECT_BG);
                std::string line = "[" + reportList[i].user + "]: \"" +
                    truncate_text(reportList[i].text, 28) + "\" (" +
                    truncate_text(reportList[i].reason, 18) + ")";
                draw_text(8, y, 0.42f, sel ? C_MID : C_DARK, line);
                y += ROW_H;
            }
        }
        return;
    }

    draw_text(8, y, 0.42f, C_MUTED, "D-Pad: Select   (X) Send   (Y) Report");
    y += 16;
    if (isAdmin) {
        draw_text(8, y, 0.42f, C_ADMIN, "(B) Admin Panel");
        y += 16;
    }
    y += 4;

    if (messageList.empty()) {
        draw_text(8, y, 0.48f, C_MUTED, "No messages available.");
    } else {
        int total = (int)messageList.size();
        int maxVisible = std::max(1, (int)((LIST_BOTTOM - y) / ROW_H));
        int scrollOffset = 0;
        if (total > maxVisible) {
            scrollOffset = selectedMsgIndex - maxVisible + 1;
            if (scrollOffset < 0) scrollOffset = 0;
            int maxOffset = total - maxVisible;
            if (scrollOffset > maxOffset) scrollOffset = maxOffset;

            char counter[32];
            snprintf(counter, sizeof(counter), "%d-%d/%d", scrollOffset + 1, std::min(total, scrollOffset + maxVisible), total);
            draw_text(SCREEN_W - 70, y - 16, 0.32f, C_MUTED, counter);
        }
        int endIdx = std::min(total, scrollOffset + maxVisible);

        for (int i = scrollOffset; i < endIdx; i++) {
            bool sel = i == selectedMsgIndex;
            if (sel) C2D_DrawRectSolid(4, y-2, 0.4f, SCREEN_W-8, 18, C_SELECT_BG);
            std::string line = "[" + messageList[i].user + "]: " + truncate_text(messageList[i].text, 42);

            bool isTechnobladeJoin = (messageList[i].user == "System") &&
                                      (messageList[i].text == TECHNOBLADE_JOIN_TEXT);
            u32 lineColor = isTechnobladeJoin ? C2D_Color32(220,30,30,255) : (sel ? C_MID : C_DARK);

            draw_text(8, y, 0.44f, lineColor, line);
            y += ROW_H;
        }
    }
}

// ---------------------------------------------------------------------
// Bottom screen: branding (fox, wordmark, link, controls)
// ---------------------------------------------------------------------
static void draw_bottom_screen() {
    const float SCREEN_W = 320.0f;

    C2D_DrawRectSolid(0, 0, 0.5f, SCREEN_W, 60, C_MID);

    draw_fox(48, 30, 64);
    draw_text(90, 8, 0.62f, C_WHITE, "FoxWebChat");
    draw_text(90, 34, 0.42f, C_CREAM, "DarkFox Co.");

    draw_text(16, 68, 0.44f, C_TEXT, "Web Edition:");
    draw_text(16, 90, 0.36f, C_MUTED, "slabylol.github.io/foxwebchat-/");

    u32 updateColor = updateAvailable ? C2D_Color32(214,150,20,255) : C_MUTED;
    draw_text(16, 108, 0.38f, updateColor, updateStatus);

    draw_text(16, 130, 0.44f, C_TEXT, "Controls:");
    draw_text(16, 150, 0.36f, C_MUTED, "D-Pad Up/Down: Select   L/R: Theme");
    draw_text(16, 168, 0.36f, C_MUTED, "(A) Join   (X) Send   (Y) Report");
    draw_text(16, 186, 0.36f, C_MUTED, "[START] - Exit");
    draw_text(16, 204, 0.36f, C_TEXT, std::string("Theme: ") + currentTheme->name);
    draw_text(16, 224, 0.32f, C_MUTED, "Unfair kick?: Contact darkfox.tobias@outlook.com");
}

// Full-screen splash (e.g. for the update check on startup)
static void draw_splash_screen(const std::string& message, u32 messageColor) {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 90, 120);
    draw_text_centered(200, 150, 0.62f, C_WHITE, "FoxWebChat");
    draw_text_centered(200, 182, 0.44f, messageColor, message);

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, message);
}

// Confirmation screen: "Update available" (black text) + (A) Download / (B) Exit
static void draw_update_prompt() {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 76, 100);
    draw_text_centered(200, 130, 0.56f, C_WHITE, "FoxWebChat");
    draw_text_centered(200, 166, 0.54f, C_BLACK, "Update available");
    draw_text_centered(200, 196, 0.38f, C_DARK, updateStatus);

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 90, 0.46f, C_WHITE, "Update required!");
    draw_text_centered(160, 130, 0.42f, C_WHITE, "(A) Download   (B) Exit");
}

// Progress bar - shown during update or theme downloads
static void draw_progress_screen(const std::string& label, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 74, 90);
    draw_text_centered(200, 122, 0.54f, C_WHITE, "FoxWebChat");
    draw_text_centered(200, 156, 0.40f, C_CREAM, label);

    float barW = 260.0f, barH = 16.0f;
    float barX = 200.0f - barW / 2.0f, barY = 180.0f;
    C2D_DrawRectSolid(barX, barY, 0.5f, barW, barH, C2D_Color32(255,255,255,90));
    C2D_DrawRectSolid(barX, barY, 0.5f, barW * progress, barH, C_WHITE);

    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100.0f));
    draw_text_centered(200, 204, 0.36f, C_CREAM, pctBuf);

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, "Please wait...");
}

// Fun little reveal shown once when the SELECT+L secret combo is triggered
static void draw_secret_unlocked_screen() {
    C2D_TargetClear(topTarget, C2D_Color32(212,175,55,255));
    C2D_SceneBegin(topTarget);
    draw_fox(200, 90, 130);
    draw_text_centered(200, 158, 0.60f, C_BLACK, "You found a secret!");
    draw_text_centered(200, 190, 0.42f, C2D_Color32(40,30,10,255), "\"Secret Fox\" theme unlocked");

    C2D_TargetClear(bottomTarget, C2D_Color32(30,20,10,255));
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 100, 0.42f, C2D_Color32(212,175,55,255), "Shh... don't tell anyone.");
    draw_text_centered(160, 130, 0.34f, C2D_Color32(255,248,225,255), "Keep pressing D-Pad L/R to find it again anytime.");
}

// Prompt: new themes found on GitHub that are still missing locally
static void draw_new_themes_prompt() {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 56, 76);
    draw_text_centered(200, 98, 0.48f, C_WHITE, "New themes available!");

    float ly = 128;
    int shown = 0;
    for (size_t i = 0; i < pendingNewThemes.size(); i++) {
        if (shown >= 5) {
            draw_text_centered(200, ly, 0.32f, C_MUTED, "...and more");
            break;
        }
        std::string fname = pendingNewThemes[i].name;
        std::string label = (fname.size() > 5) ? fname.substr(0, fname.size() - 5) : fname;
        draw_text_centered(200, ly, 0.36f, C_DARK, label);
        ly += 16;
        shown++;
    }

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 90, 0.44f, C_WHITE, "Download new themes?");
    draw_text_centered(160, 126, 0.40f, C_WHITE, "(A) Download   (B) Skip");
}

// Result screen after downloading new themes
static void draw_theme_download_result(bool success, int downloadedCount, const std::string& detail) {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_text_centered(200, 90, 0.50f, success ? C_WHITE : C2D_Color32(255,140,140,255),
                        success ? "Themes downloaded!" : "Theme download failed");
    if (success) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d new theme(s) added.", downloadedCount);
        draw_text_centered(200, 124, 0.38f, C_CREAM, buf);
        draw_text_centered(200, 156, 0.34f, C_MUTED, "Use D-Pad Left/Right to try them.");
    } else {
        draw_text_centered(200, 124, 0.36f, C_CREAM, detail);
    }

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, "Press any button to continue");
}

// Download result
static void draw_download_result(bool success, const std::string& detail) {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_text_centered(200, 74, 0.50f, success ? C_WHITE : C2D_Color32(255,140,140,255),
                        success ? "Download complete!" : "Download failed");
    if (!success) {
        draw_text_centered(200, 104, 0.36f, C_CREAM, detail);
    }
    if (success) {
        draw_text_centered(200, 140, 0.38f, C_CREAM, "Saved to:");
        draw_text_centered(200, 164, 0.34f, C_WHITE, "/3ds/FoxWebChat/FoxWebChat.cia");
        draw_text_centered(200, 196, 0.34f, C_MUTED, "Install it with FBI, then relaunch.");
    }

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, "Press any button to continue");
}

int main(int argc, char **argv) {
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    dynamicBuf = C2D_TextBufNew(4096);

    load_secret_flag();
    init_themes();
    load_theme();

    // Initialize sockets & CURL
    u32 *soc_buffer = (u32*)memalign(0x1000, 0x100000);
    if (soc_buffer != NULL) socInit(soc_buffer, 0x100000);
    curl_global_init(CURL_GLOBAL_ALL);

    // Splash screen while checking for updates
    C2D_TextBufClear(dynamicBuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        draw_splash_screen("Checking for Updates...", C_CREAM);
    C3D_FrameEnd(0);

    check_for_updates();

    if (updateAvailable) {
        // The update is mandatory: (A) downloads it, (B) exits the app directly.
        // EITHER way the app quits afterwards, it never continues into the chat.
        bool waitingForChoice = true;
        while (waitingForChoice && aptMainLoop()) {
            hidScanInput();
            u32 kd = hidKeysDown();

            C2D_TextBufClear(dynamicBuf);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                draw_update_prompt();
            C3D_FrameEnd(0);

            if (kd & KEY_A) {
                // Show the progress screen while downloading
                g_downloadProgress = 0.0f;
                g_downloadLabel = "Downloading update...";
                C2D_TextBufClear(dynamicBuf);
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                    draw_progress_screen(g_downloadLabel, g_downloadProgress);
                C3D_FrameEnd(0);

                std::string errorDetail;
                bool success = download_update_cia(errorDetail);

                bool showingResult = true;
                while (showingResult && aptMainLoop()) {
                    hidScanInput();
                    u32 kd2 = hidKeysDown();

                    C2D_TextBufClear(dynamicBuf);
                    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                        draw_download_result(success, errorDetail);
                    C3D_FrameEnd(0);

                    if (kd2 & (KEY_A | KEY_B | KEY_START)) showingResult = false;
                }
                waitingForChoice = false;
            } else if (kd & KEY_B) {
                waitingForChoice = false;
            }
        }

        // Mandatory update: cleanly quit the app here in every case instead of continuing
        curl_global_cleanup();
        socExit();
        if (soc_buffer) free(soc_buffer);
        C2D_TextBufDelete(dynamicBuf);
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 0;
    } else {
        // Briefly show "Up to date" / "Update check failed" before entering the app
        C2D_TextBufClear(dynamicBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            draw_splash_screen(updateStatus, C_CREAM);
        C3D_FrameEnd(0);
        svcSleepThread(1200000000LL); // 1.2 seconds

        // Check the GitHub repo for new themes (themes/*.fwct)
        C2D_TextBufClear(dynamicBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            draw_splash_screen("Checking for new themes...", C_CREAM);
        C3D_FrameEnd(0);

        check_for_new_themes();

        if (!pendingNewThemes.empty()) {
            bool waitingThemeChoice = true;
            while (waitingThemeChoice && aptMainLoop()) {
                hidScanInput();
                u32 kdTheme = hidKeysDown();

                C2D_TextBufClear(dynamicBuf);
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                    draw_new_themes_prompt();
                C3D_FrameEnd(0);

                if (kdTheme & KEY_A) {
                    int downloadedCount = 0;
                    std::string lastError;
                    bool allOk = true;

                    for (size_t i = 0; i < pendingNewThemes.size(); i++) {
                        char label[64];
                        snprintf(label, sizeof(label), "Theme %d/%d: %s",
                                 (int)(i + 1), (int)pendingNewThemes.size(), pendingNewThemes[i].name.c_str());
                        g_downloadLabel = label;
                        g_downloadProgress = 0.0f;

                        C2D_TextBufClear(dynamicBuf);
                        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                            draw_progress_screen(g_downloadLabel, g_downloadProgress);
                        C3D_FrameEnd(0);

                        std::string err;
                        if (download_theme_file(pendingNewThemes[i], err)) {
                            downloadedCount++;
                        } else {
                            allOk = false;
                            lastError = err;
                        }
                    }

                    // Rebuild the theme list so the new themes are available right away
                    init_themes();
                    if (currentThemeIndex >= (int)allThemes.size()) currentThemeIndex = 0;
                    currentTheme = &allThemes[currentThemeIndex];

                    bool showingResult = true;
                    while (showingResult && aptMainLoop()) {
                        hidScanInput();
                        u32 kdResult = hidKeysDown();

                        C2D_TextBufClear(dynamicBuf);
                        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                            draw_theme_download_result(allOk, downloadedCount, lastError);
                        C3D_FrameEnd(0);

                        if (kdResult & (KEY_A | KEY_B | KEY_START)) showingResult = false;
                    }
                    waitingThemeChoice = false;
                } else if (kdTheme & KEY_B) {
                    waitingThemeChoice = false;
                }
            }
        }
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if ((kDown & KEY_SELECT) && isAdmin) {
            firebase_post("messages", "{\"user\":\"System\",\"text\":\"Something....\"}");
        }

        // Switch theme (possible at any time, even before joining)
        if (kDown & KEY_DRIGHT) {
            currentThemeIndex = (currentThemeIndex + 1) % (int)allThemes.size();
            currentTheme = &allThemes[currentThemeIndex];
            save_theme();
        }
        if (kDown & KEY_DLEFT) {
            currentThemeIndex = (currentThemeIndex - 1 + (int)allThemes.size()) % (int)allThemes.size();
            currentTheme = &allThemes[currentThemeIndex];
            save_theme();
        }

        // Secret: hold SELECT + L together to unlock a hidden bonus theme
        {
            u32 kHeld = hidKeysHeld();
            bool comboPressed = ((kDown & KEY_SELECT) && (kHeld & KEY_L)) ||
                                 ((kDown & KEY_L) && (kHeld & KEY_SELECT));
            if (comboPressed && !showAdminPanel) {
                bool wasAlreadyUnlocked = secretThemeUnlocked;
                unlock_secret_theme();
                init_themes();
                currentThemeIndex = (int)allThemes.size() - 1; // the secret theme is always appended last
                currentTheme = &allThemes[currentThemeIndex];
                save_theme();

                if (!wasAlreadyUnlocked) {
                    bool showingSecret = true;
                    while (showingSecret && aptMainLoop()) {
                        hidScanInput();
                        u32 kdSecret = hidKeysDown();
                        C2D_TextBufClear(dynamicBuf);
                        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                            draw_secret_unlocked_screen();
                        C3D_FrameEnd(0);
                        if (kdSecret & (KEY_A | KEY_B | KEY_START)) showingSecret = false;
                    }
                }
            }
        }

        if (osGetTime() - lastFetchTime > 5000 && strlen(username) > 0 && !isKicked) {
            check_kick_status();
            fetch_messages();
            if (isAdmin) fetch_reports();
            lastFetchTime = osGetTime();
        }

        if (!isKicked) {
            // JOIN (A)
            if ((kDown & KEY_A) && strlen(username) == 0) {
                char inputName[64] = "";
                open_keyboard(inputName, sizeof(inputName), "Enter Name");
                if (strlen(inputName) > 0) {
                    std::string lowerName = inputName;
                    for (auto& c : lowerName) c = (char)tolower((unsigned char)c);

                    if (strcmp(inputName, ADMIN_CODE) == 0) {
                        isAdmin = true;
                        strcpy(username, "ADMIN");
                        firebase_post("messages", "{\"user\":\"System\",\"text\":\"ADMIN JOINED!\"}");
                    } else if (lowerName == "technoblade") {
                        isAdmin = false;
                        strcpy(username, "Technoblade");
                        firebase_post("messages", "{\"user\":\"System\",\"text\":\"" TECHNOBLADE_JOIN_TEXT "\"}");
                    } else {
                        isAdmin = false;
                        strcpy(username, inputName);
                        char msg[128];
                        snprintf(msg, sizeof(msg), "{\"user\":\"System\",\"text\":\"%s joined\"}", username);
                        firebase_post("messages", msg);
                    }
                    fetch_messages();
                    if (isAdmin) fetch_reports();
                }
            }

            // TOGGLE ADMIN PANEL (B)
            if ((kDown & KEY_B) && isAdmin) {
                showAdminPanel = !showAdminPanel;
                if(showAdminPanel) fetch_reports();
            }

            // ADMIN ACTIONS WITHIN THE ADMIN PANEL
            if (showAdminPanel && isAdmin) {
                if (kDown & KEY_Y) {
                    firebase_delete("reports");
                    fetch_reports();
                }

                if (kDown & KEY_L) {
                    firebase_delete("messages");
                    firebase_post("messages", "{\"user\":\"System\",\"text\":\"All messages cleared by Admin!\"}");
                    fetch_messages();
                }

                if (kDown & KEY_R) {
                    firebase_delete("kicks");
                    firebase_post("messages", "{\"user\":\"System\",\"text\":\"All kick data cleared by Admin!\"}");
                }
            }

            // SEND MESSAGE / KICK (X)
            if ((kDown & KEY_X) && strlen(username) > 0) {
                if (showAdminPanel && isAdmin && !reportList.empty()) {
                    ReportItem rep = reportList[selectedReportIndex];

                    char kickPath[128];
                    snprintf(kickPath, sizeof(kickPath), "kicks/%s", rep.user.c_str());
                    firebase_put(kickPath, "{\"kicked\":true}");

                    char sysMsg[256];
                    snprintf(sysMsg, sizeof(sysMsg), "{\"user\":\"System\",\"text\":\"%s was kicked!\"}", rep.user.c_str());
                    firebase_post("messages", sysMsg);

                    fetch_reports();
                } else if (!showAdminPanel) {
                    char text[256] = "";
                    open_keyboard(text, sizeof(text), "Message...");
                    if (strlen(text) > 0) {
                        char payload[512];
                        snprintf(payload, sizeof(payload), "{\"user\":\"%s\",\"text\":\"%s\"}", username, text);
                        firebase_post("messages", payload);
                        followLatestMsg = true;
                        fetch_messages();
                    }
                }
            }

            // NAVIGATION (disables auto-scroll as soon as the user manually scrolls up)
            if (kDown & KEY_DUP) {
                if (showAdminPanel && selectedReportIndex > 0) {
                    selectedReportIndex--;
                } else if (!showAdminPanel && selectedMsgIndex > 0) {
                    selectedMsgIndex--;
                    followLatestMsg = false;
                }
            }
            if (kDown & KEY_DDOWN) {
                if (showAdminPanel && selectedReportIndex < (int)reportList.size() - 1) {
                    selectedReportIndex++;
                } else if (!showAdminPanel && selectedMsgIndex < (int)messageList.size() - 1) {
                    selectedMsgIndex++;
                    if (selectedMsgIndex == (int)messageList.size() - 1) followLatestMsg = true;
                }
            }

            // SEND REPORT (Y - with Uno Reverse protection for the admin)
            if ((kDown & KEY_Y) && !messageList.empty() && strlen(username) > 0 && !showAdminPanel) {
                ChatMessage selected = messageList[selectedMsgIndex];
                char reason[128] = "";
                open_keyboard(reason, sizeof(reason), "Report Reason...");

                if (strlen(reason) > 0) {
                    char reportPayload[512];

                    // Protects the admin: the reporter gets reported instead!
                    if (selected.user == "ADMIN") {
                        char customReason[256];
                        snprintf(customReason, sizeof(customReason), "[UNO REVERSE] Tried to report Admin! Reason: %s", reason);

                        snprintf(reportPayload, sizeof(reportPayload),
                            "{\"reportedUser\":\"%s\",\"messageText\":\"%s\",\"reason\":\"%s\",\"reporter\":\"%s\"}",
                            username, selected.text.c_str(), customReason, username);
                    } else {
                        snprintf(reportPayload, sizeof(reportPayload),
                            "{\"reportedUser\":\"%s\",\"messageText\":\"%s\",\"reason\":\"%s\",\"reporter\":\"%s\"}",
                            selected.user.c_str(), selected.text.c_str(), reason, username);
                    }

                    firebase_post("reports", reportPayload);
                }
            }
        }

        // ---- Drawing ----
        C2D_TextBufClear(dynamicBuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C2D_TargetClear(topTarget, C_WHITE);
            C2D_SceneBegin(topTarget);
            draw_top_screen();

            C2D_TargetClear(bottomTarget, C_BG);
            C2D_SceneBegin(bottomTarget);
            draw_bottom_screen();
        C3D_FrameEnd(0);
    }

    curl_global_cleanup();
    socExit();
    if (soc_buffer) free(soc_buffer);
    C2D_TextBufDelete(dynamicBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
