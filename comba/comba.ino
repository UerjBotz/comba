#define COMBA

#include <Arduino.h>
#include "../comms.h"
#define LED D4
#define motor_esq_m1 D5
#define motor_esq_m2 D6
#define motor_dir_m1 D7
#define motor_dir_m2 D8
#include "../robot.h"

#define BAUD_RATE 115200
#define IDX_VEL 0
#define IDX_ESC 1

#define memeql(a,b,sz) (memcmp(a,b,sz) == 0)
#define LEN(arr) (sizeof(arr)/sizeof(*arr))

struct vel { int16_t esq = 0, dir = 0; }; //! nomes
union vels {
    int16_t   raw[6];
    struct vel of[3];
};

union vels str_to_vels(char *const text, uint8_t len) {
    const char sep = ' ';
 
    uint8_t v = 1, seps[6] = {0};
    for (uint i = 0; i < len; i++) {
        if (text[i] == sep) seps[v++] = i;

        if (text[i] == '\0') break;
        if (v  >= LEN(seps)) break;
    }

    union vels vels{0};
    for (uint i = 0; i < LEN(seps); i++) {
        vels.raw[i] = atoi(&text[seps[i]]);
    }
    return vels;
}

union vels vels{0};
void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
    Packet* msg = (Packet*) (void*)data;
    vels = str_to_vels(msg->vels, msg->len);
    
    //! print
    // Serial.printf("Received %d bytes: msg type %d\n", msg->len, msg->id);
}

void setup() {
    robot_setup();
    init_wifi();
    uint8_t* mac_addr = get_mac_addr();

    Serial.begin(BAUD_RATE);
    Serial.printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    esp_now_register_recv_cb(esp_now_recv_cb_t(on_recv));
}

void loop() {
    static struct vel vel_rodas, vel_esc;
    vel_rodas = vels.of[IDX_VEL];
    vel_esc   = vels.of[IDX_ESC];

    move(vel_rodas.esq, vel_rodas.dir);
    hite(vel_esc.esq);
    
    //! print
    Serial.printf("vels %d %d, esc %d %d, não usado %d %d\n",
                  vels.of[0].esq, vels.of[0].dir,
                  vels.of[1].esq, vels.of[1].dir,
                  vels.of[2].esq, vels.of[2].dir);
    Serial.println();

    yield();
}
