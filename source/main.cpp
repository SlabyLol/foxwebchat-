#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <vector>
#include <string>
#include <curl/curl.h>

#define FIREBASE_URL "https://foxwebchat-bd592-default-rtdb.europe-west1.firebasedatabase.app"
#define ADMIN_CODE "AdminJs93€=no"

// ---------------------------------------------------------------------
// Farbpalette (passend zu Icon/Banner: warmes Orange, flaches Design)
// ---------------------------------------------------------------------
#define C_BG        C2D_Color32(247,127,51,255)   // Haupt-Orange
#define C_MID       C2D_Color32(225,90,35,255)    // dunkleres Orange (Akzent)
#define C_WHITE     C2D_Color32(255,255,255,255)
#define C_CREAM     C2D_Color32(255,247,240,255)
#define C_DARK      C2D_Color32(61,33,26,255)      // Text / Augen
#define C_SELECT_BG C2D_Color32(255,213,181,255)   // Auswahl-Hervorhebung
#define C_ADMIN     C2D_Color32(214,40,40,255)
#define C_MUTED     C2D_Color32(120,90,80,255)
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
    std::string json = firebase_get("messages");
    messageList.clear();

    if(json.empty() || json == "null") return;

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

void fetch_reports() {
    std::string json = firebase_get("reports");
    reportList.clear();

    if(json.empty() || json == "null") return;

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
            for (size_t i = 0; i < reportList.size() && y < 230; i++) {
                bool sel = (int)i == selectedReportIndex;
                if (sel) C2D_DrawRectSolid(4, y-2, 0.4f, SCREEN_W-8, 18, C_SELECT_BG);
                std::string line = "[" + reportList[i].user + "]: \"" +
                    truncate_text(reportList[i].text, 28) + "\" (" +
                    truncate_text(reportList[i].reason, 18) + ")";
                draw_text(8, y, 0.42f, sel ? C_MID : C_DARK, line);
                y += 18;
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
        for (size_t i = 0; i < messageList.size() && y < 230; i++) {
            bool sel = (int)i == selectedMsgIndex;
            if (sel) C2D_DrawRectSolid(4, y-2, 0.4f, SCREEN_W-8, 18, C_SELECT_BG);
            std::string line = "[" + messageList[i].user + "]: " + truncate_text(messageList[i].text, 42);
            draw_text(8, y, 0.44f, sel ? C_MID : C_DARK, line);
            y += 18;
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

    draw_text(16, 74, 0.42f, C_DARK, "Web Edition:");
    draw_text(16, 93, 0.36f, C_MUTED, "slabylol.github.io/foxwebchat-/");

    u32 updateColor = updateAvailable ? C2D_Color32(214,150,20,255) : C_MUTED;
    draw_text(16, 113, 0.36f, updateColor, updateStatus);

    draw_text(16, 154, 0.40f, C_DARK, "Controls:");
    draw_text(16, 172, 0.36f, C_MUTED, "D-Pad Up/Down - Select message");
    draw_text(16, 189, 0.36f, C_MUTED, "(A) Join   (X) Send   (Y) Report");
    draw_text(16, 206, 0.36f, C_MUTED, "[START] - Exit");
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

// Bestaetigungs-Screen: "Update available" (schwarzer Text) + (A) Yes / (B) No
static void draw_update_prompt() {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_fox(200, 76, 100);
    draw_text_centered(200, 130, 0.56f, C_WHITE, "FoxWebChat");
    draw_text_centered(200, 166, 0.54f, C_BLACK, "Update available");
    draw_text_centered(200, 196, 0.38f, C_DARK, updateStatus);

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 90, 0.46f, C_WHITE, "Install update?");
    draw_text_centered(160, 130, 0.42f, C_WHITE, "(A) Yes    (B) No");
}

// Hinweis-Screen mit der Download-Seite, nachdem (A) Yes gedrueckt wurde
static void draw_update_download_info() {
    C2D_TargetClear(topTarget, C_BG);
    C2D_SceneBegin(topTarget);
    draw_text_centered(200, 80, 0.46f, C_WHITE, "Open this page to download:");
    draw_text_centered(200, 112, 0.40f, C_CREAM, "github.com/SlabyLol/foxwebchat-");
    draw_text_centered(200, 134, 0.40f, C_CREAM, "/releases/tag/nightly");
    draw_text_centered(200, 176, 0.36f, C_MUTED, "Install the .cia with FBI, then");
    draw_text_centered(200, 194, 0.36f, C_MUTED, "press any button to continue");

    C2D_TargetClear(bottomTarget, C_MID);
    C2D_SceneBegin(bottomTarget);
    draw_text_centered(160, 110, 0.40f, C_WHITE, "Press A / B / START to continue");
}

int main(int argc, char **argv) {
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    dynamicBuf = C2D_TextBufNew(4096);

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
        // Bestaetigungs-Dialog: (A) Yes -> Download-Hinweis, (B) No -> weiter zur App
        bool waitingForChoice = true;
        while (waitingForChoice && aptMainLoop()) {
            hidScanInput();
            u32 kd = hidKeysDown();

            C2D_TextBufClear(dynamicBuf);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                draw_update_prompt();
            C3D_FrameEnd(0);

            if (kd & KEY_A) {
                bool showingInfo = true;
                while (showingInfo && aptMainLoop()) {
                    hidScanInput();
                    u32 kd2 = hidKeysDown();

                    C2D_TextBufClear(dynamicBuf);
                    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                        draw_update_download_info();
                    C3D_FrameEnd(0);

                    if (kd2 & (KEY_A | KEY_B | KEY_START)) showingInfo = false;
                }
                waitingForChoice = false;
            } else if (kd & KEY_B) {
                waitingForChoice = false;
            }
        }
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
                        fetch_messages();
                    }
                }
            }

            // NAVIGATION
            if (kDown & KEY_DUP) {
                if (showAdminPanel && selectedReportIndex > 0) {
                    selectedReportIndex--;
                } else if (!showAdminPanel && selectedMsgIndex > 0) {
                    selectedMsgIndex--;
                }
            }
            if (kDown & KEY_DDOWN) {
                if (showAdminPanel && selectedReportIndex < (int)reportList.size() - 1) {
                    selectedReportIndex++;
                } else if (!showAdminPanel && selectedMsgIndex < (int)messageList.size() - 1) {
                    selectedMsgIndex++;
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
