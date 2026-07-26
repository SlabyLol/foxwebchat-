#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>
#include <vector>
#include <string>
#include <curl/curl.h>

#define FIREBASE_URL "https://foxwebchat-bd592-default-rtdb.europe-west1.firebasedatabase.app"
#define ADMIN_CODE "AdminJs93€=no"
#define UPDATE_CIA_URL "https://github.com/SlabyLol/foxwebchat-/releases/download/nightly/FoxWebChat.cia"
#define UPDATE_CIA_DIR "sdmc:/3ds/FoxWebChat"
#define UPDATE_CIA_PATH "sdmc:/3ds/FoxWebChat/FoxWebChat.cia"
#define THEME_CFG_PATH "sdmc:/3ds/FoxWebChat/theme.cfg"

// ---------------------------------------------------------------------
// Themes: mehrere Farbschemata, durchschaltbar mit D-Pad Links/Rechts
// ---------------------------------------------------------------------
struct Theme {
    const char* name;
    u32 bg;        // Haupt-Akzentfarbe (Kopfzeile, unterer Bildschirm)
    u32 mid;       // dunklere Akzentfarbe
    u32 white;     // Fuchs / helle Flaechen
    u32 cream;     // sekundaerer heller Text
    u32 dark;      // Haupttext
    u32 selectBg;  // Hervorhebung ausgewaehlter Zeilen
    u32 admin;     // Admin-/Warnfarbe
    u32 muted;     // gedaempfter Hinweistext
};

static const Theme THEMES[] = {
    { "Orange",       C2D_Color32(247,127,51,255), C2D_Color32(225,90,35,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,247,240,255),
                       C2D_Color32(61,33,26,255),   C2D_Color32(255,213,181,255),
                       C2D_Color32(214,40,40,255),  C2D_Color32(120,90,80,255) },

    { "Blau/Violett",  C2D_Color32(88,80,220,255),  C2D_Color32(58,50,168,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(238,236,255,255),
                       C2D_Color32(28,24,58,255),   C2D_Color32(205,200,255,255),
                       C2D_Color32(230,60,60,255),  C2D_Color32(150,145,195,255) },

    { "Feuerrot",      C2D_Color32(210,58,40,255),  C2D_Color32(165,38,26,255),
                       C2D_Color32(255,255,255,255), C2D_Color32(255,233,228,255),
                       C2D_Color32(48,20,16,255),   C2D_Color32(255,190,175,255),
                       C2D_Color32(255,205,0,255),  C2D_Color32(150,90,80,255) },

    { "Dunkel",        C2D_Color32(38,38,44,255),   C2D_Color32(22,22,26,255),
                       C2D_Color32(235,235,240,255), C2D_Color32(200,200,210,255),
                       C2D_Color32(235,235,240,255), C2D_Color32(95,95,115,255),
                       C2D_Color32(255,90,90,255),  C2D_Color32(150,150,162,255) },
};
static const int THEME_COUNT = sizeof(THEMES) / sizeof(THEMES[0]);
static int currentThemeIndex = 0;
static const Theme* currentTheme = &THEMES[0];

// ---------------------------------------------------------------------
// Farbpalette - liest sich immer aus dem aktuell gewaehlten Theme
// ---------------------------------------------------------------------
#define C_BG        (currentTheme->bg)
#define C_MID       (currentTheme->mid)
#define C_WHITE     (currentTheme->white)
#define C_CREAM     (currentTheme->cream)
#define C_DARK      (currentTheme->dark)
#define C_SELECT_BG (currentTheme->selectBg)
#define C_ADMIN     (currentTheme->admin)
#define C_MUTED     (currentTheme->muted)
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

// Auto-Scroll: solange true, springt die Auswahl bei neuen Nachrichten automatisch
// zur neuesten Nachricht (wie bei einem normalen Chat). Wird deaktiviert, sobald
// der Nutzer manuell nach oben scrollt, und wieder aktiviert, sobald er unten ankommt.
static bool followLatestMsg = true;

// ---------------------------------------------------------------------
// citro2d Render-Ziele & Text-Puffer
// ---------------------------------------------------------------------
static C3D_RenderTarget* topTarget;
static C3D_RenderTarget* bottomTarget;
static C2D_TextBuf dynamicBuf;

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
        // ignorieren - falls es wirklich fehlschlaegt, scheitert der spaetere fopen() ohnehin
    }
}

// Theme-Wahl auf der SD-Karte speichern / laden
static void load_theme() {
    FILE* fp = fopen(THEME_CFG_PATH, "r");
    if (fp) {
        int idx = -1;
        if (fscanf(fp, "%d", &idx) == 1 && idx >= 0 && idx < THEME_COUNT) {
            currentThemeIndex = idx;
        }
        fclose(fp);
    }
    currentTheme = &THEMES[currentThemeIndex];
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

// Laedt die aktuelle .cia von der GitHub-Release-Seite herunter und speichert sie
// unter /3ds/FoxWebChat/FoxWebChat.cia auf der SD-Karte.
static bool download_update_cia(std::string& statusOut) {
    ensure_dir("sdmc:/3ds");
    ensure_dir(UPDATE_CIA_DIR);

    FILE* fp = fopen(UPDATE_CIA_PATH, "wb");
    if (!fp) {
        statusOut = "Could not open file for writing";
        return false;
    }

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

    // Auto-Scroll: solange der Nutzer bei der neuesten Nachricht "mitliest",
    // automatisch zur jeweils neuesten Nachricht springen.
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

// Erwartet in Firebase unter dem Pfad "version" einen simplen String-Wert,
// z.B. per Firebase-Konsole: version: "1.0.1"
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
// Zeichen-Helfer
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

// Fuchs-Kopf im flachen Stil (passend zu Icon & Banner), rein vektoriell gezeichnet
static void draw_fox(float cx, float cy, float w) {
    // Ohren (aussen weiss)
    C2D_DrawTriangle(cx-w*0.46f, cy-w*0.62f, C_WHITE,
                      cx-w*0.62f, cy-w*0.14f, C_WHITE,
                      cx-w*0.08f, cy-w*0.30f, C_WHITE, 0.5f);
    C2D_DrawTriangle(cx+w*0.46f, cy-w*0.62f, C_WHITE,
                      cx+w*0.62f, cy-w*0.14f, C_WHITE,
                      cx+w*0.08f, cy-w*0.30f, C_WHITE, 0.5f);
    // Ohren innen (Akzentfarbe)
    C2D_DrawTriangle(cx-w*0.40f, cy-w*0.46f, C_MID,
                      cx-w*0.49f, cy-w*0.22f, C_MID,
                      cx-w*0.20f, cy-w*0.30f, C_MID, 0.5f);
    C2D_DrawTriangle(cx+w*0.40f, cy-w*0.46f, C_MID,
                      cx+w*0.49f, cy-w*0.22f, C_MID,
                      cx+w*0.20f, cy-w*0.30f, C_MID, 0.5f);
    // Kopf
    C2D_DrawEllipseSolid(cx - w*0.72f/2, cy - w*0.02f - w*0.62f/2, 0.5f, w*0.72f, w*0.62f, C_WHITE);
    // Schnauze
    C2D_DrawTriangle(cx-w*0.14f, cy+w*0.02f, C_BG,
                      cx+w*0.14f, cy+w*0.02f, C_BG,
                      cx,         cy+w*0.30f, C_BG, 0.5f);
    // Augen
    C2D_DrawCircleSolid(cx-w*0.16f, cy-w*0.02f, 0.5f, w*0.045f, C_DARK);
    C2D_DrawCircleSolid(cx+w*0.16f, cy-w*0.02f, 0.5f, w*0.045f, C_DARK);
    // Nase
    C2D_DrawTriangle(cx-w*0.045f, cy+w*0.14f, C_DARK,
                      cx+w*0.045f, cy+w*0.14f, C_DARK,
                      cx,          cy+w*0.14f+w*0.045f*1.3f, C_DARK, 0.5f);
}

// ---------------------------------------------------------------------
// Oberer Bildschirm: Titel, Statuszeilen, Nachrichten-/Report-Liste
// ---------------------------------------------------------------------
static void draw_top_screen() {
    const float SCREEN_W = 400.0f;
    const float HEADER_H = 34.0f;
    const float LIST_BOTTOM = 230.0f;
    const int ROW_H = 18;

    // Kopfzeile
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
            draw_text(8, y, 0.44f, sel ? C_MID : C_DARK, line);
            y += ROW_H;
        }
    }
}

// ---------------------------------------------------------------------
// Unterer Bildschirm: Branding (Fuchs, Wortmarke, Link, Steuerung)
// ---------------------------------------------------------------------
static void draw_bottom_screen() {
    const float SCREEN_W = 320.0f;

    C2D_DrawRectSolid(0, 0, 0.5f, SCREEN_W, 60, C_MID);

    draw_fox(48, 30, 64);
    draw_text(90, 8, 0.62f, C_WHITE, "FoxWebChat");
    draw_text(90, 34, 0.40f, C_CREAM, "DarkFox Co.");

    draw_text(16, 68, 0.38f, C_DARK, "Web Edition:");
    draw_text(16, 85, 0.32f, C_MUTED, "slabylol.github.io/foxwebchat-/");

    u32 updateColor = updateAvailable ? C2D_Color32(214,150,20,255) : C_MUTED;
    draw_text(16, 102, 0.34f, updateColor, updateStatus);

    draw_text(16, 122, 0.38f, C_DARK, "Controls:");
    draw_text(16, 138, 0.32f, C_MUTED, "D-Pad Up/Down: Select   L/R: Theme");
    draw_text(16, 154, 0.32f, C_MUTED, "(A) Join   (X) Send   (Y) Report");
    draw_text(16, 170, 0.32f, C_MUTED, "[START] - Exit");
    draw_text(16, 186, 0.32f, C_MUTED, std::string("Theme: ") + currentTheme->name);
    draw_text(16, 206, 0.28f, C_MUTED, "Unfair kick?: Contact darkfox.tobias@outlook.com");
}

// Vollflaechiger Splash-Screen (z.B. fuer den Update-Check beim Start)
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

// Bestaetigungs-Screen: "Update available" (schwarzer Text) + (A) Download / (B) Exit
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

// Waehrend des Downloads
static void draw_downloading_screen() {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 90, 110);
    draw_text_centered(200, 150, 0.58f, C_WHITE, "FoxWebChat");
    draw_text_centered(200, 182, 0.44f, C_CREAM, "Downloading update...");

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, "Please wait...");
}

// Ergebnis des Downloads
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

    load_theme();

    // Sockets & CURL initialisieren
    u32 *soc_buffer = (u32*)memalign(0x1000, 0x100000);
    if (soc_buffer != NULL) socInit(soc_buffer, 0x100000);
    curl_global_init(CURL_GLOBAL_ALL);

    // Splash-Screen waehrend des Update-Checks
    C2D_TextBufClear(dynamicBuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        draw_splash_screen("Checking for Updates...", C_CREAM);
    C3D_FrameEnd(0);

    check_for_updates();

    if (updateAvailable) {
        // Update ist Pflicht: (A) laedt herunter, (B) beendet die App direkt.
        // In BEIDEN Faellen wird die App danach beendet, nicht weiter fortgesetzt.
        bool waitingForChoice = true;
        while (waitingForChoice && aptMainLoop()) {
            hidScanInput();
            u32 kd = hidKeysDown();

            C2D_TextBufClear(dynamicBuf);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                draw_update_prompt();
            C3D_FrameEnd(0);

            if (kd & KEY_A) {
                // Download-Screen anzeigen, waehrend heruntergeladen wird
                C2D_TextBufClear(dynamicBuf);
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                    draw_downloading_screen();
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

        // Pflicht-Update: App hier in jedem Fall sauber beenden statt fortzusetzen
        curl_global_cleanup();
        socExit();
        if (soc_buffer) free(soc_buffer);
        C2D_TextBufDelete(dynamicBuf);
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 0;
    } else {
        // "Up to date" / "Update check failed" kurz anzeigen, bevor es in die App geht
        C2D_TextBufClear(dynamicBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            draw_splash_screen(updateStatus, C_CREAM);
        C3D_FrameEnd(0);
        svcSleepThread(1200000000LL); // 1.2 Sekunden
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if ((kDown & KEY_SELECT) && isAdmin) {
            firebase_post("messages", "{\"user\":\"System\",\"text\":\"Something....\"}");
        }

        // Theme wechseln (jederzeit moeglich, auch vor dem Beitreten)
        if (kDown & KEY_DRIGHT) {
            currentThemeIndex = (currentThemeIndex + 1) % THEME_COUNT;
            currentTheme = &THEMES[currentThemeIndex];
            save_theme();
        }
        if (kDown & KEY_DLEFT) {
            currentThemeIndex = (currentThemeIndex - 1 + THEME_COUNT) % THEME_COUNT;
            currentTheme = &THEMES[currentThemeIndex];
            save_theme();
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
                    if (strcmp(inputName, ADMIN_CODE) == 0) {
                        isAdmin = true;
                        strcpy(username, "ADMIN");
                        firebase_post("messages", "{\"user\":\"System\",\"text\":\"ADMIN JOINED!\"}");
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

            // ADMIN AKTIONEN IM ADMIN-PANEL
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

            // NACHRICHT SENDEN / KICKEN (X)
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

            // NAVIGATION (deaktiviert Auto-Scroll, sobald man manuell nach oben blaettert)
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

            // REPORT SENDEN (Y - Mit Uno Reverse Schutz fuer den Admin)
            if ((kDown & KEY_Y) && !messageList.empty() && strlen(username) > 0 && !showAdminPanel) {
                ChatMessage selected = messageList[selectedMsgIndex];
                char reason[128] = "";
                open_keyboard(reason, sizeof(reason), "Report Reason...");

                if (strlen(reason) > 0) {
                    char reportPayload[512];

                    // Schuetzt den Admin: Melder wird selbst gemeldet!
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

        // ---- Zeichnen ----
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
