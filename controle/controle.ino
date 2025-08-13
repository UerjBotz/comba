#include <Arduino.h>
#include "../comms.h"

#define MIXAR
#define PONTO_ZERO { .x = 2815, .y = 2290 }

#define BAUD_RATE 9600
#define ADC_MAX ((1<<12)-1)

//#define PWM_MAX ((1<<10)-1)
#define PWM_MAX 127

#define EIXO_X 2
#define EIXO_Y 1
#define BOTAO  9

#define PEER_ADDR gesonel

struct par {
    union {
        struct { int16_t x, y; };
        struct { int16_t esq, dir; };
    };
};

volatile bool inverter = false;
volatile unsigned long ultimo_clique = 0;
void IRAM_ATTR ao_apertar(void) {
    unsigned long t = millis();
    if ((t - ultimo_clique) > 1000) {
        inverter ^= 1;
        ultimo_clique = t;
    }
}

void setup() {
    pinMode(EIXO_X, INPUT);
    pinMode(EIXO_Y, INPUT);
    pinMode(BOTAO, INPUT);

    init_wifi();
    uint8_t* mac_addr = get_mac_addr();

    Serial.begin(BAUD_RATE);
    Serial.printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    static esp_now_peer_info_t peer {
        .channel = 0,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, PEER_ADDR, sizeof(PEER_ADDR));
    
    esp_err_t err = esp_now_add_peer(&peer);
    assert (err == ESP_OK);

    attachInterrupt(digitalPinToInterrupt(BOTAO), ao_apertar, HIGH);
}


void loop() {
    static const char sentinel = '\n';
    static char input[255] = {0};
    for (auto buf = input; Serial.available(); buf++) {
        *buf = (char) Serial.read();
        if (*buf == sentinel) {
            *buf = '\0';
            esp_err_t err = send_str(PEER_ADDR, input);
            
            //! print
            Serial.printf("%s ", input); 
            if (err == ESP_OK) Serial.printf("-> Serial: success\n");
            else               Serial.printf("-> Serial: error\n");
        }
    }

    //bool inverter = !digitalRead(BOTAO);

    struct par pos = {
        .x = analogRead(EIXO_X),
        .y = analogRead(EIXO_Y),
    };
    struct par pos_norm = deadzone(pos.x, pos.y);
    struct par pos_pwm = {
        .x = adc_to_pwm(pos_norm.x),
        .y = adc_to_pwm(pos_norm.y),
    };
    if (inverter) {
        pos_pwm.x = pos_pwm.x;
        pos_pwm.y = -pos_pwm.y;
    }
    struct par vel = mixar(pos_pwm.x, pos_pwm.y);

    char vels[255];
    sprintf(vels, "%d %d\n", vel.x, vel.y);
    esp_err_t err = send_str(PEER_ADDR, vels);

    Serial.printf("%5d,%5d: ", pos.x, pos.y);
    Serial.printf("%5d,%5d: ", pos_norm.x, pos_norm.y);
    Serial.printf("%5d,%5d: ", pos_pwm.x, pos_pwm.y);
    Serial.printf("%5d,%5d " , vel.esq, vel.dir);
    if (err == ESP_OK) Serial.printf("-> Joystick: success\n");
    else               Serial.printf("-> Joystick: error\n");
}

esp_err_t send_str(uint8_t addr[6], const char* str) {
    Packet msg {
        .id  = 0,
        .len = (uint8_t)strlen(str),
    };
    strcpy(msg.vels, str);
    
    uint8_t*  ptr = (uint8_t*) &msg;
    esp_err_t err = esp_now_send(addr, ptr, sizeof(msg));
    
    return err;
}

struct par deadzone(int16_t x, int16_t y) {
    const struct par zero = PONTO_ZERO;
    return {
        .x = (x > zero.x) ?
             map(x, zero.x,ADC_MAX, ADC_MAX/2, ADC_MAX) :
             map(x, 0,     zero.x,  0,       ADC_MAX/2),
        .y = (y > zero.y) ?
             map(y, zero.y,ADC_MAX, ADC_MAX/2, ADC_MAX) :
             map(y, 0,     zero.y,  0,       ADC_MAX/2),
    };
}
int16_t adc_to_pwm(unsigned long adc) {
    return map(adc, 0,ADC_MAX, -PWM_MAX,PWM_MAX);
}
struct par mixar(int16_t x, int16_t y) {
  #ifdef MIXAR
    return {
        .esq = constrain(y + x, -PWM_MAX,PWM_MAX),
        .dir = constrain(y - x, -PWM_MAX,PWM_MAX),
    };
  #else
    return { .esq = x, .dir = y };
  #endif
}
