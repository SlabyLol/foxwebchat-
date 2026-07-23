#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <vector>
#include <string>
#include <curl/curl.h>

#define FIREBASE_URL "https://foxwebchat-bd592-default-rtdb.europe-west1.firebasedatabase.app"
#define ADMIN_CODE "AdminJs93€=no"

struct ChatMessage {
    std::string user;
    std::string text;
};

struct ReportItem {
    std::string key;
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

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// POST / PUT Request to Firebase
void firebase_put(const char* path, const char* json_data) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void firebase_post(const char* path, const char* json_data) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char full_url[256];
        snprintf(full_url, sizeof(full_url), "%s/%s.json", FIREBASE_URL, path);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

// GET Request to Firebase
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
        
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

// Check if current user is kicked
void check_kick_status() {
    if(strlen(username) == 0) return;
    
    char path[128];
    snprintf(path, sizeof(path), "kicks/%s", username);
    std::string json = firebase_get(path);
    
    if(!json.empty() && json != "null") {
        isKicked = true;
        size_t pos = json.find("\"reason\":");
        if(pos != std::string::npos) {
            size_t start = json.find("\"", pos + 9) + 1;
            size_t end = json.find("\"", start);
            std::string reasonStr = json.substr(start, end - start);
            snprintf(kickReason, sizeof(kickReason), "%s", reasonStr.c_str());
        } else {
            snprintf(kickReason, sizeof(kickReason), "Kicked by an Admin");
        }
    }
}

// Fetch messages from Firebase
void fetch_messages() {
    std::string json = firebase_get("messages");
    if(json.empty() || json == "null") return;

    messageList.clear();
    size_t pos = 0;
    while((pos = json.find("\"text\":", pos)) != std::string::npos) {
        size_t textStart = json.find("\"", pos + 7) + 1;
        size_t textEnd = json.find("\"", textStart);
        std::string text = json.substr(textStart, textEnd - textStart);

        size_t userPos = json.find("\"user\":", pos);
        std::string user = "Unknown";
        if(userPos != std::string::npos) {
            size_t userStart = json.find("\"", userPos + 7) + 1;
            size_t userEnd = json.find("\"", userStart);
            user = json.substr(userStart, userEnd - userStart);
        }

        messageList.push_back({user, text});
        pos = textEnd;
    }
    
    if(selectedMsgIndex >= (int)messageList.size()) {
        selectedMsgIndex = messageList.size() > 0 ? messageList.size() - 1 : 0;
    }
}

// Fetch reports from Firebase (Admins only)
void fetch_reports() {
    if(!isAdmin) return;
    std::string json = firebase_get("reports");
    if(json.empty() || json == "null") {
        reportList.clear();
        return;
    }

    reportList.clear();
    size_t pos = 0;
    while((pos = json.find("\"reportedUser\":", pos)) != std::string::npos) {
        size_t userStart = json.find("\"", pos + 15) + 1;
        size_t userEnd = json.find("\"", userStart);
        std::string user = json.substr(userStart, userEnd - userStart);

        size_t textPos = json.find("\"messageText\":", pos);
        std::string text = "N/A";
        if(textPos != std::string::npos) {
            size_t textStart = json.find("\"", textPos + 14) + 1;
            size_t textEnd = json.find("\"", textStart);
            text = json.substr(textStart, textEnd - textStart);
        }

        size_t reasonPos = json.find("\"reason\":", pos);
        std::string reason = "No reason";
        if(reasonPos != std::string::npos) {
            size_t reasonStart = json.find("\"", reasonPos + 9) + 1;
            size_t reasonEnd = json.find("\"", reasonStart);
            reason = json.substr(reasonStart, reasonEnd - reasonStart);
        }

        reportList.push_back({"", user, text, reason});
        pos = userEnd;
    }

    if(selectedReportIndex >= (int)reportList.size()) {
        selectedReportIndex = reportList.size() > 0 ? reportList.size() - 1 : 0;
    }
}

void open_keyboard(char* out_text, size_t max_len, const char* hint) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hint);
    swkbdInputText(&swkbd, out_text, max_len);
}

void redraw_screen() {
    consoleClear();
    
    if (isKicked) {
        printf("\x1b[16;3H\x1b[31mOops! You got kicked by an Admin!\x1b[0m\n");
        printf("\x1b[18;3HReason: %s\n", kickReason);
        return;
    }

    printf("\x1b[1;1HFoxWebChat 3DS - User: %s\n", strlen(username) > 0 ? username : "Not joined");
    printf("==================================================\n");

    if (strlen(username) == 0) {
        printf("Press (A) to enter name / join\n");
        return;
    }

    // ADMIN PANEL VIEW
    if (showAdminPanel) {
        printf("\x1b[31m=== ADMIN PANEL ===\x1b[0m\n");
        printf("(B): Close | (X): Kick Reported User\n");
        printf("--------------------------------------------------\n");
        if (reportList.empty()) {
            printf("No active reports available.\n");
        } else {
            for(size_t i = 0; i < reportList.size(); i++) {
                if((int)i == selectedReportIndex) {
                    printf("\x1b[31m> [%s]: \"%s\" (Reason: %s)\x1b[0m\n", 
                        reportList[i].user.c_str(), reportList[i].text.c_str(), reportList[i].reason.c_str());
                } else {
                    printf("  [%s]: \"%s\" (Reason: %s)\n", 
                        reportList[i].user.c_str(), reportList[i].text.c_str(), reportList[i].reason.c_str());
                }
            }
        }
        return;
    }

    // CHAT VIEW
    printf("D-Pad [UP/DOWN]: Select Message\n");
    printf("(X): Send Message | (Y): Report Message\n");
    if (isAdmin) {
        printf("\x1b[33m(B): Open Admin Panel\x1b[0m\n");
    }
    printf("--------------------------------------------------\n");

    if (messageList.empty()) {
        printf("No messages available.\n");
    } else {
        for (size_t i = 0; i < messageList.size(); i++) {
            if ((int)i == selectedMsgIndex) {
                printf("\x1b[32m> [%s]: %s\x1b[0m\n", messageList[i].user.c_str(), messageList[i].text.c_str());
            } else {
                printf("  [%s]: %s\n", messageList[i].user.c_str(), messageList[i].text.c_str());
            }
        }
    }
}

int main(int argc, char **argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    
    u32 *soc_buffer = (u32*)memalign(0x1000, 0x100000);
    if (soc_buffer != NULL) socInit(soc_buffer, 0x100000);
    curl_global_init(CURL_GLOBAL_ALL);

    redraw_screen();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        // Auto-refresh every 3 seconds
        if (osGetTime() - lastFetchTime > 3000 && strlen(username) > 0 && !isKicked) {
            check_kick_status();
            fetch_messages();
            if (isAdmin) fetch_reports();
            lastFetchTime = osGetTime();
            redraw_screen();
        }

        if (!isKicked) {
            // JOIN (A)
            if ((kDown & KEY_A) && strlen(username) == 0) {
                open_keyboard(username, sizeof(username), "Enter Name");
                if (strlen(username) > 0) {
                    if (strcmp(username, ADMIN_CODE) == 0) {
                        isAdmin = true;
                        strcpy(username, "ADMIN");
                        firebase_post("messages", "{\"user\":\"System\",\"text\":\"ADMIN JOINED - WARNING!\"}");
                    } else {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "{\"user\":\"System\",\"text\":\"%s joined\"}", username);
                        firebase_post("messages", msg);
                    }
                    check_kick_status();
                    fetch_messages();
                    redraw_screen();
                }
            }

            // TOGGLE ADMIN PANEL (B)
            if ((kDown & KEY_B) && isAdmin) {
                showAdminPanel = !showAdminPanel;
                if(showAdminPanel) fetch_reports();
                redraw_screen();
            }

            // SEND MESSAGE (X) / KICK USER IN ADMIN PANEL (X)
            if ((kDown & KEY_X) && strlen(username) > 0) {
                if (showAdminPanel && isAdmin && !reportList.empty()) {
                    ReportItem rep = reportList[selectedReportIndex];
                    
                    // Kick user in DB
                    char kickPath[128];
                    snprintf(kickPath, sizeof(kickPath), "kicks/%s", rep.user.c_str());
                    char kickPayload[256];
                    snprintf(kickPayload, sizeof(kickPayload), "{\"reason\":\"%s\"}", rep.reason.c_str());
                    firebase_put(kickPath, kickPayload);

                    // Send system notification
                    char sysMsg[256];
                    snprintf(sysMsg, sizeof(sysMsg), "{\"user\":\"System\",\"text\":\"%s got kicked. Reason: %s\"}", rep.user.c_str(), rep.reason.c_str());
                    firebase_post("messages", sysMsg);

                    fetch_reports();
                    redraw_screen();
                } else if (!showAdminPanel) {
                    char text[256] = "";
                    open_keyboard(text, sizeof(text), "Message...");
                    if (strlen(text) > 0) {
                        char payload[512];
                        snprintf(payload, sizeof(payload), "{\"user\":\"%s\",\"text\":\"%s\"}", username, text);
                        firebase_post("messages", payload);
                        fetch_messages();
                        redraw_screen();
                    }
                }
            }

            // D-PAD NAVIGATION (UP / DOWN)
            if (kDown & KEY_DUP) {
                if (showAdminPanel && selectedReportIndex > 0) {
                    selectedReportIndex--;
                    redraw_screen();
                } else if (!showAdminPanel && selectedMsgIndex > 0) {
                    selectedMsgIndex--;
                    redraw_screen();
                }
            }
            if (kDown & KEY_DDOWN) {
                if (showAdminPanel && selectedReportIndex < (int)reportList.size() - 1) {
                    selectedReportIndex++;
                    redraw_screen();
                } else if (!showAdminPanel && selectedMsgIndex < (int)messageList.size() - 1) {
                    selectedMsgIndex++;
                    redraw_screen();
                }
            }

            // REPORT MESSAGE (Y)
            if ((kDown & KEY_Y) && !messageList.empty() && strlen(username) > 0 && !showAdminPanel) {
                ChatMessage selected = messageList[selectedMsgIndex];

                if (!isAdmin && selected.user == "ADMIN") {
                    char shameMsg[256];
                    snprintf(shameMsg, sizeof(shameMsg), "{\"user\":\"System\",\"text\":\"%s reported an Admin! What a shame!\"}", username);
                    firebase_post("messages", shameMsg);

                    char reportPayload[256];
                    snprintf(reportPayload, sizeof(reportPayload), "{\"reportedUser\":\"%s\",\"messageText\":\"Tried to report ADMIN\",\"reason\":\"Attempted to report Admin\",\"reporter\":\"%s\"}", username, username);
                    firebase_post("reports", reportPayload);

                    isKicked = true;
                    strcpy(kickReason, "Attempted to report Admin");
                } else {
                    char reason[128] = "";
                    open_keyboard(reason, sizeof(reason), "Report Reason...");
                    if (strlen(reason) > 0) {
                        char reportPayload[512];
                        snprintf(reportPayload, sizeof(reportPayload), "{\"reportedUser\":\"%s\",\"messageText\":\"%s\",\"reason\":\"%s\",\"reporter\":\"%s\"}", selected.user.c_str(), selected.text.c_str(), reason, username);
                        firebase_post("reports", reportPayload);
                    }
                }
                redraw_screen();
            }
        } else {
            redraw_screen();
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    curl_global_cleanup();
    socExit();
    if (soc_buffer) free(soc_buffer);
    gfxExit();
    return 0;
}
