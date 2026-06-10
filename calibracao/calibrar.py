#!/usr/bin/env python3
"""
Assistente de calibração e diagnóstico dos pedais G27.

Par do firmware "diagnostico_serial/diagnostico_serial.ino", que emite
linhas "RAW,millis,A0,A1,A2,A3" a 115200 baud.

O assistente conduz o teste pedal por pedal (solto e pisado), mede
ruído e vazamento entre canais, sugere valores de CAL_MIN/CAL_MAX
para o firmware HID e grava tudo em um arquivo de log.

Uso:
    pip install pyserial
    python calibrar.py            # detecta a porta automaticamente
    python calibrar.py COM5       # ou indique a porta
"""

import statistics
import sys
import time
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("Biblioteca pyserial ausente. Instale com: pip install pyserial")

BAUD = 115200
CANAIS = ["A0 Acelerador", "A1 Freio", "A2 Embreagem", "A3 Freio de Mao"]
PEDAIS = [0, 1, 2]          # canais testados pedal a pedal (A3 só observado)
DURACAO_AMOSTRA_S = 3.0     # tempo de coleta em cada etapa
LIMIAR_VAZAMENTO = 5        # variação (counts) num canal parado p/ acusar vazamento


def escolher_porta(arg):
    if arg:
        return arg
    portas = list(serial.tools.list_ports.comports())
    if not portas:
        sys.exit("Nenhuma porta serial encontrada. O Pro Micro esta conectado?")
    # Pro Micro/Leonardo aparece como "Arduino" ou VID 2341/1b4f
    for p in portas:
        desc = (p.description or "").lower()
        if "arduino" in desc or "leonardo" in desc or "micro" in desc:
            return p.device
    if len(portas) == 1:
        return portas[0].device
    print("Portas encontradas:")
    for i, p in enumerate(portas):
        print(f"  [{i}] {p.device} - {p.description}")
    idx = int(input("Escolha o numero da porta: "))
    return portas[idx].device


def coletar(ser, duracao_s):
    """Coleta linhas RAW por `duracao_s` e devolve lista de [a0,a1,a2,a3]."""
    amostras = []
    ser.reset_input_buffer()
    fim = time.monotonic() + duracao_s
    while time.monotonic() < fim:
        linha = ser.readline().decode("ascii", errors="ignore").strip()
        if not linha.startswith("RAW,"):
            continue
        partes = linha.split(",")
        if len(partes) != 6:
            continue
        try:
            amostras.append([int(v) for v in partes[2:6]])
        except ValueError:
            continue
    return amostras


def resumo(amostras, canal):
    valores = [a[canal] for a in amostras]
    return {
        "min": min(valores),
        "max": max(valores),
        "media": round(statistics.mean(valores), 1),
        "desvio": round(statistics.pstdev(valores), 1),
        "n": len(valores),
    }


def log_e_print(arq, texto=""):
    print(texto)
    arq.write(texto + "\n")


def main():
    porta = escolher_porta(sys.argv[1] if len(sys.argv) > 1 else None)
    nome_log = datetime.now().strftime("log_calibracao_%Y%m%d_%H%M%S.txt")

    with serial.Serial(porta, BAUD, timeout=1) as ser, \
            open(nome_log, "w", encoding="utf-8") as arq:
        lp = lambda t="": log_e_print(arq, t)

        lp("=" * 60)
        lp("  G27 Pedals - Assistente de Calibracao")
        lp(f"  Porta: {porta}   Data: {datetime.now():%d/%m/%Y %H:%M:%S}")
        lp("=" * 60)

        time.sleep(2)  # tempo do firmware reiniciar apos abrir a porta
        if not coletar(ser, 1.0):
            lp("ERRO: nenhum dado RAW recebido. O sketch diagnostico_serial"
               " esta gravado no Arduino?")
            sys.exit(1)

        # ── Etapa 1: repouso geral ────────────────────────────────
        input("\n>> Solte TODOS os pedais e pressione ENTER...")
        lp("\n[REPOUSO GERAL] coletando 3 s...")
        repouso = coletar(ser, DURACAO_AMOSTRA_S)
        rep = [resumo(repouso, c) for c in range(4)]
        for c in range(4):
            r = rep[c]
            lp(f"  {CANAIS[c]:<16} media={r['media']:>6}  min={r['min']:>4}"
               f"  max={r['max']:>4}  ruido(desvio)={r['desvio']}")

        # ── Etapa 2: cada pedal pisado ────────────────────────────
        resultados = {}
        for c in PEDAIS:
            input(f"\n>> Pise FUNDO no {CANAIS[c]} e SEGURE. ENTER para medir...")
            lp(f"\n[{CANAIS[c]} PISADO] coletando 3 s...")
            pisado = coletar(ser, DURACAO_AMOSTRA_S)
            p = resumo(pisado, c)
            lp(f"  {CANAIS[c]:<16} media={p['media']:>6}  min={p['min']:>4}"
               f"  max={p['max']:>4}  ruido(desvio)={p['desvio']}")

            # Vazamento: quanto os OUTROS canais mexeram em relacao ao repouso
            for o in range(4):
                if o == c:
                    continue
                outro = resumo(pisado, o)
                delta = abs(outro["media"] - rep[o]["media"])
                if delta >= LIMIAR_VAZAMENTO:
                    lp(f"    !! VAZAMENTO em {CANAIS[o]}: media moveu "
                       f"{rep[o]['media']} -> {outro['media']} (delta {delta:.1f})")
            resultados[c] = p
            input(f">> Pode soltar o {CANAIS[c]}. ENTER para continuar...")

        # ── Etapa 3: analise e sugestao ───────────────────────────
        lp("\n" + "=" * 60)
        lp("  ANALISE")
        lp("=" * 60)
        cal_min, cal_max = [60] * 4, [960] * 4
        for c in PEDAIS:
            solto = rep[c]["media"]
            pisado = resultados[c]["media"]
            span = abs(solto - pisado)
            lo, hi = sorted([solto, pisado])
            margem = max(3, int(span * 0.05))
            cal_min[c] = int(lo + margem)
            cal_max[c] = int(hi - margem)
            lp(f"\n{CANAIS[c]}: solto={solto}  pisado={pisado}  curso={span:.0f}")
            if span < 50:
                lp("  !! CURSO MUITO CURTO (<50 counts): sinal fraco - "
                   "suspeite de VCC ausente no potenciometro ou pot com defeito.")
            if solto < 700 and pisado < 700:
                lp("  !! LEITURAS BAIXAS: com 5V no pot, o repouso deveria "
                   "ficar acima de ~700. Verifique o fio de VCC.")
            if rep[c]["desvio"] > 3 or resultados[c]["desvio"] > 3:
                lp("  !! RUIDO ALTO (desvio > 3): verifique aterramento/cabo.")

        lp("\nSugestao para o g27_pedals_hid.ino:")
        lp(f"const int CAL_MIN[4] = {{ {cal_min[0]:>4}, {cal_min[1]:>4},"
           f" {cal_min[2]:>4}, {cal_min[3]:>4} }};")
        lp(f"const int CAL_MAX[4] = {{ {cal_max[0]:>4}, {cal_max[1]:>4},"
           f" {cal_max[2]:>4}, {cal_max[3]:>4} }};")
        lp(f"\nLog completo salvo em: {nome_log}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrompido.")
