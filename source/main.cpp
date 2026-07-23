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

static char username[64] = "";
static bool isAdmin = false;
static bool isKicked = false;
static char kickReason[128] = "";

std::vector<ChatMessage> messageList;
int selectedMsgIndex = 0;

// HTTP Response Callback for cURL
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Send Data to Firebase
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

// 3DS Tastatur aufrufen
void open_keyboard(char* out_text, size_t max_len, const char* hint) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hint);
    swkbdInputText(&swkbd, out_text, max_len);
}

// Screen Redraw Function
void redraw_screen() {
    consoleClear();
    
    if (isKicked) {
        printf("\x1b[16;5H\x1b[31mOops! You got kicked by an Admin!\x1b[0m\n");
        printf("\x1b[18;5HReason: %s\n", kickReason);
        return;
    }

    printf("\x1b[1;1HFoxWebChat 3DS - User: %s\n", strlen(username) > 0 ? username : "Not joined");
    printf("==================================================\n");

    if (strlen(username) == 0) {
        printf("Press (A) to Join Chat\n");
        return;
    }

    printf("Steuerkreuz [UP/DOWN]: Nachricht waehlen\n");
    printf("(X): Nachricht senden | (Y): Ausgewaehlte Msg Reporten\n");
    printf("--------------------------------------------------\n");

    if (messageList.empty()) {
        printf("Keine Nachrichten vorhanden.\n");
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

        if (!isKicked) {
            // JOIN
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
                    redraw_screen();
                }
            }

            // SEND MESSAGE
            if ((kDown & KEY_X) && strlen(username) > 0) {
                char text[256] = "";
                open_keyboard(text, sizeof(text), "Message...");
                if (strlen(text) > 0) {
                    char payload[512];
                    snprintf(payload, sizeof(payload), "{\"user\":\"%s\",\"text\":\"%s\"}", username, text);
                    firebase_post("messages", payload);
                    
                    // Lokale Anzeige ergaenzen
                    messageList.push_back({username, text});
                    selectedMsgIndex = messageList.size() - 1;
                    redraw_screen();
                }
            }

            // NACHRICHTEN NAVIGATION (Steuerkreuz Hoch / Runter)
            if (kDown & KEY_DUP) {
                if (selectedMsgIndex > 0) {
                    selectedMsgIndex--;
                    redraw_screen();
                }
            }
            if (kDown & KEY_DDOWN) {
                if (selectedMsgIndex < (int)messageList.size() - 1) {
                    selectedMsgIndex++;
                    redraw_screen();
                }
            }

            // REPORT SELECTED MESSAGE (Mit Admin-Shame-Mechanic!)
            if ((kDown & KEY_Y) && !messageList.empty() && strlen(username) > 0) {
                ChatMessage selected = messageList[selectedMsgIndex];

                if (!isAdmin && selected.user == "ADMIN") {
                    // Nutzer versucht ADMIN zu reporten -> Shame Mechanic!
                    char shameMsg[256];
                    snprintf(shameMsg, sizeof(shameMsg), "{\"user\":\"System\",\"text\":\"%s reported an Admin! What a shame!\"}", username);
                    firebase_post("messages", shameMsg);

                    char reportPayload[256];
                    snprintf(reportPayload, sizeof(reportPayload), "{\"reportedUser\":\"%s\",\"messageText\":\"Tried to report ADMIN\",\"reason\":\"Attempted to report Admin\",\"reporter\":\"%s\"}", username, username);
                    firebase_post("reports", reportPayload);

                    isKicked = true;
                    strcpy(kickReason, "Attempted to report Admin");
                } else {
                    // Normaler Report
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
