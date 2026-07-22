#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define FIREBASE_URL "https://foxwebchat-bd592-default-rtdb.europe-west1.firebasedatabase.app"
#define ADMIN_CODE "AdminJs93€=no"

static char username[64] = "";
static bool isAdmin = false;
static bool isKicked = false;
static char kickReason[128] = "";

// HTTP POST Helper for Firebase REST API
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
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Allow 3DS SSL handshake
        
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

// Open 3DS On-Screen Keyboard
void open_keyboard(char* out_text, size_t max_len, const char* hint) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hint);
    swkbdInputText(&swkbd, out_text, max_len);
}

// Black Screen on Kick
void render_kick_screen() {
    consoleClear();
    printf("\x1b[16;5H\x1b[31mOops! You got kicked by an Admin!\x1b[0m\n");
    printf("\x1b[18;5HReason: %s\n", kickReason);
}

int main(int argc, char **argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    
    // Init SOC for 3DS Internet
    u32 *soc_buffer = (u32*)memalign(0x1000, 0x100000);
    socInit(soc_buffer, 0x100000);
    curl_global_init(CURL_GLOBAL_ALL);

    printf("\x1b[1;1HFoxWebChat - 3DS Edition\n");
    printf("==================================\n\n");
    printf("Press (A) to Enter Name / Join\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (isKicked) {
            render_kick_screen();
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
            continue;
        }

        if (kDown & KEY_START) break;

        // JOIN ACTION
        if ((kDown & KEY_A) && strlen(username) == 0) {
            open_keyboard(username, sizeof(username), "Enter Name");
            
            if (strlen(username) > 0) {
                if (strcmp(username, ADMIN_CODE) == 0) {
                    isAdmin = true;
                    strcpy(username, "ADMIN");
                    printf("\x1b[31mADMIN JOINED - WARNING!\x1b[0m\n");
                    firebase_post("messages", "{\"user\":\"System\",\"text\":\"ADMIN JOINED - WARNING!\"}");
                } else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "{\"user\":\"System\",\"text\":\"%s joined\"}", username);
                    firebase_post("messages", msg);
                    printf("Joined as: %s\n", username);
                    printf("Press (X) to Send Message | (Y) to Report Admin (Shame test)\n");
                }
            }
        }

        // SEND MESSAGE ACTION
        if ((kDown & KEY_X) && strlen(username) > 0) {
            char text[256] = "";
            open_keyboard(text, sizeof(text), "Message...");
            if (strlen(text) > 0) {
                char payload[512];
                snprintf(payload, sizeof(payload), "{\"user\":\"%s\",\"text\":\"%s\"}", username, text);
                firebase_post("messages", payload);
                printf("[%s]: %s\n", username, text);
            }
        }

        // SHAME MECHANIC TEST (Report Admin Button Logic)
        if ((kDown & KEY_Y) && strlen(username) > 0 && !isAdmin) {
            printf("\x1b[31m[!] Executing Admin Report Shame Mechanic...\x1b[0m\n");
            
            char shameMsg[256];
            snprintf(shameMsg, sizeof(shameMsg), "{\"user\":\"System\",\"text\":\"%s reported an Admin! What a shame!\"}", username);
            firebase_post("messages", shameMsg);

            char reportPayload[256];
            snprintf(reportPayload, sizeof(reportPayload), "{\"reportedUser\":\"%s\",\"messageText\":\"Tried to report ADMIN\",\"reason\":\"Attempted to report Admin\",\"reporter\":\"%s\"}", username, username);
            firebase_post("reports", reportPayload);

            // Set kick state locally & server
            isKicked = true;
            strcpy(kickReason, "Attempted to report Admin");
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    curl_global_cleanup();
    socExit();
    gfxExit();
    return 0;
}
