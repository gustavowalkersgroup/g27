/*
 * G27 Pedals - Diagnóstico Serial (formato de máquina)
 * Placa: Arduino Pro Micro (ATmega32U4, 5V, 16MHz)
 *
 * Par do assistente "calibracao/calibrar.py": emite as leituras
 * brutas dos 4 canais analógicos em CSV, uma linha a cada 20 ms:
 *
 *   RAW,<millis>,<A0>,<A1>,<A2>,<A3>
 *
 * Não usa HID — grave este sketch só durante o diagnóstico e
 * depois regrave o g27_pedals_hid.
 *
 * Compilação: Ferramentas → Placa → Arduino Leonardo, Upload.
 */

const int PINOS[4] = {A0, A1, A2, A3};
const unsigned long INTERVALO_MS = 20;

unsigned long ultimaLeitura = 0;

/*
 * Leitura dupla com descarte: o ADC único do ATmega32U4 retém
 * carga do canal anterior no capacitor de sample-and-hold; a
 * primeira leitura só assenta o canal, a segunda é a válida.
 */
int lerAnalogico(int pino) {
    analogRead(pino);
    delayMicroseconds(100);
    return analogRead(pino);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }  // aguarda o host USB conectar
    Serial.println(F("# G27 diagnostico serial v1"));
    Serial.println(F("# RAW,millis,A0,A1,A2,A3"));
}

void loop() {
    unsigned long agora = millis();
    if (agora - ultimaLeitura < INTERVALO_MS) return;
    ultimaLeitura = agora;

    Serial.print(F("RAW,"));
    Serial.print(agora);
    for (int i = 0; i < 4; i++) {
        Serial.print(',');
        Serial.print(lerAnalogico(PINOS[i]));
    }
    Serial.println();
}
