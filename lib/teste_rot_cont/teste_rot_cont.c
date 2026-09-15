#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <pwm_z42.h>
#include <servo_motor.h>       /* a camada: usada SO pela medicao de posicao */
#include <pisca_led.h>          /* o LED da placa como indicador de fase */
#include <teste_rot_cont.h>

/* ---------------------------------------------------------------------------
   A aritmetica, toda derivada da base de tempo do TPM.

   Com clock de 375 kHz e MOD = 7500, o periodo e 20 ms (50 Hz) e cada
   milissegundo de pulso vale 375 contagens. Para servo de rotacao continua:

       1,0 ms = 375 contagens -> velocidade maxima, sentido A
       1,5 ms = 563 contagens -> PARADO (neutro)
       2,0 ms = 750 contagens -> velocidade maxima, sentido B

   Logo a excursao de velocidade e +-188 contagens em torno do neutro, e "x% da
   velocidade" e simplesmente x% dessas 188.
   --------------------------------------------------------------------------- */
#define CNV_NEUTRO        563   /* 1,50 ms - parado                            */
#define EXCURSAO_MAX      188   /* 0,50 ms - do neutro ate a velocidade maxima */

/* ⚠️ Percentual BAIXO pode nao ser suficiente para o servo sair do lugar.
   Servo de rotacao continua tem ZONA MORTA em torno do neutro, tipicamente de
   +-45 a +-90 us; 10% de 0,50 ms sao 50 us - dentro dessa faixa em muitos
   servos.

   ⭐ Se o eixo nao se mexer, NAO e defeito: suba este numero. O primeiro valor
   em que ele comeca a girar E a zona morta medida, e vale anotar. */
#define VELOCIDADE_PCT     10

#define PASSO_CNV        ((EXCURSAO_MAX * VELOCIDADE_PCT) / 100)
#define CNV_SENTIDO_A    (CNV_NEUTRO - PASSO_CNV)
#define CNV_SENTIDO_B    (CNV_NEUTRO + PASSO_CNV)

#define CANAL_HORIZONTAL   3    /* TPM0_CH3 - PTD3 */
#define CANAL_VERTICAL     0    /* TPM0_CH0 - PTD0 */

#define DEGRAU_MS        1000

/* Aciona os DOIS canais: assim o teste funciona com o servo em qualquer um dos
   dois conectores, sem precisar adivinhar qual esta ligado. */
static void aplicar(const char *rotulo, uint16_t cnv)
{
    printk("[rot-cont] %-22s CnV=%3u  (%d,%02d ms)\n",
           rotulo, cnv, cnv / 375, ((cnv % 375) * 100) / 375);

    pwm_tpm_CnV(TPM0, CANAL_HORIZONTAL, cnv);
    pwm_tpm_CnV(TPM0, CANAL_VERTICAL,   cnv);
    k_msleep(DEGRAU_MS);
}

/* ⭐ RECIBO DE GRAVACAO: tres piscadas rapidas na cor desta gravacao.

   Mora AQUI, e nao em pisca_led/, porque a biblioteca do LED tem uma funcao
   publica so - ela segura um nivel, e piscar e um gesto de quem chama. Os dois
   testes deste arquivo usam, entao vale uma definicao em vez de dois lacos
   copiados.

   ⚠️ Se a cor nao mudou depois de gravar, o upload NAO chegou ao alvo. Trocar
   COR_DESTA_GRAVACAO em pisca_led.h e o unico passo do ritual. */
static void recibo(void)
{
    for (int i = 0; i < 3; i++) {
        led(COR_DESTA_GRAVACAO);
        k_msleep(250);
        led(LED_APAGADO);
        k_msleep(250);
    }
}

void teste_rot_cont(int n)
{
    printk("\n=== TESTE DE ROTACAO CONTINUA ===\n");
    printk("Pulso = VELOCIDADE, nao angulo. Neutro=%u, passo=%u (%d%% da "
           "velocidade maxima)\n", CNV_NEUTRO, PASSO_CNV, VELOCIDADE_PCT);
    printk("Se o eixo nao girar, suba VELOCIDADE_PCT: percentual baixo cai\n"
           "dentro da zona morta de muitos servos.\n\n");

    recibo();   /* pisca o LED: prova que esta gravacao chegou */

    /* n == 0 e INFINITO, seguindo a convencao que teste_console() estabeleceu.
       Os quatro testes do projeto usam a mesma - "0 = nao faz nada" num e
       "0 = para sempre" noutro seria a pior armadilha possivel numa API que
       parece igual. */
    for (int i = 0; n == 0 || i < n; i++) {
        led(COR_DESTA_GRAVACAO);
        aplicar("sentido A, devagar",  CNV_SENTIDO_A);
        led(LED_APAGADO);
        aplicar("parado (neutro)",     CNV_NEUTRO);
        led(COR_DESTA_GRAVACAO);
        aplicar("sentido B, devagar",  CNV_SENTIDO_B);
        led(LED_APAGADO);
        aplicar("parado (neutro)",     CNV_NEUTRO);
    }
}

/* --- Medicao de omega: CnV cru, por fora da camada ------------------------- */

#define OMEGA_MS_GIRO      5000   /* 5 s girando: o varrido a medir      */
#define OMEGA_MS_PAUSA     8000   /* 8 s parado: tempo de marcar e medir */

/* ⚠️ A velocidade da MEDICAO tem de ser a MESMA da OPERACAO, senao o omega
   medido nao descreve o movimento real. Por isso ela NAO e um numero proprio:
   sai do VELOCIDADE_PCT deste arquivo, que deve espelhar o VELOCIDADE_PCT_H/V
   da camada de servo.

   ⛔ Os dois ja divergiram uma vez - este teste ficou em 30 enquanto a camada
   foi para 10 -, e o comentario que avisava disso nao impediu. Repetir o numero
   e o mecanismo da divergencia; derivar e a defesa. */
#define OMEGA_PASSO        PASSO_CNV

static void medir_um(const char *eixo, uint16_t canal,
                     const char *sentido, uint16_t cnv)
{
    printk("\n[omega] %s / %s: MARQUE a posicao agora (%d s)\n",
           eixo, sentido, OMEGA_MS_PAUSA / 1000);
    led(LED_APAGADO);
    k_msleep(OMEGA_MS_PAUSA);

    printk("[omega] %s / %s: GIRANDO por %d s...\n",
           eixo, sentido, OMEGA_MS_GIRO / 1000);
    led(COR_DESTA_GRAVACAO);
    pwm_tpm_CnV(TPM0, canal, cnv);
    k_msleep(OMEGA_MS_GIRO);
    pwm_tpm_CnV(TPM0, canal, CNV_NEUTRO);
    led(LED_APAGADO);

    printk("[omega] %s / %s: PARADO. Meca o varrido; omega = angulo / %d s\n",
           eixo, sentido, OMEGA_MS_GIRO / 1000);
}

void teste_rot_cont_medir_omega(int n)
{
    printk("\n=== MEDICAO DE OMEGA (graus/s) ===\n");
    printk("LED ACESO = girando. LED APAGADO = marque ou meca.\n");
    printk("Sao QUATRO numeros: cada eixo em cada sentido - servo de rotacao\n");
    printk("continua e assimetrico, e um omega so embute erro que acumula.\n");

    recibo();

    /* n == 0 e INFINITO - ver a nota em teste_rot_cont().

       ⚠️ Uma passada JA DA os quatro numeros: este teste e um ritual humano
       (8 s parado para voce marcar, 5 s girando para voce medir o arco), e
       repetir so repete a mesma medida. Cada repeticao custa 52 s. */
    for (int i = 0; n == 0 || i < n; i++) {
        medir_um("horizontal", CANAL_HORIZONTAL, "sentido A",
                 CNV_NEUTRO + OMEGA_PASSO);
        medir_um("horizontal", CANAL_HORIZONTAL, "sentido B",
                 CNV_NEUTRO - OMEGA_PASSO);
        medir_um("vertical",   CANAL_VERTICAL,   "sentido A",
                 CNV_NEUTRO + OMEGA_PASSO);
        medir_um("vertical",   CANAL_VERTICAL,   "sentido B",
                 CNV_NEUTRO - OMEGA_PASSO);
    }
}

/* --- Medicao de posicao: o unico teste que passa PELA CAMADA --------------- */

#define POSICAO_PASSO_MS    500   /* duracao de cada comando                  */
#define POSICAO_PAUSA_MS   1000   /* parado entre varreduras                  */

/* ⚠️ Teto de seguranca, nao criterio de parada: quem para a varredura e o
   LIMITE DE CURSO da camada, devolvendo 0. Se a varredura chegar a este teto,
   e sinal de que a camada NAO esta saturando - omega subestimado, limite que
   nunca chega. Chegar aqui e um achado, nao um fim normal. */
#define POSICAO_MAX_PASSOS   60

/* Uma varredura num sentido, ate a camada recusar. Devolve quantos comandos
   foram aceitos - numero util para conferir contra a conta:

       graus por comando = POSICAO_PASSO_MS * omega / 1000
       comandos ate o limite = LIMITE do eixo / graus por comando

   ⚠️ Os numeros nao ficam anotados aqui: omega e os limites de curso estao em
   ajuste, e comentario que cita valor derivado envelhece a cada build. */
static int varrer(const char *eixo, const char *sentido, servo_dir_t dir,
                  uint32_t (*mover)(servo_dir_t, uint32_t),
                  int32_t (*theta_d10)(void))
{
    int comandos = 0;

    printk("[posicao] %s: varrendo no %s\n", eixo, sentido);
    led(COR_DESTA_GRAVACAO);

    for (int i = 0; i < POSICAO_MAX_PASSOS; i++) {
        uint32_t aplicado = mover(dir, POSICAO_PASSO_MS);

        printk("  pedido=%u ms  aplicado=%u ms  theta=%+d decimos de grau%s\n",
               (unsigned)POSICAO_PASSO_MS, (unsigned)aplicado, theta_d10(),
               (aplicado == 0)               ? "   <- LIMITE: sem curso restante"
             : (aplicado < POSICAO_PASSO_MS) ? "   <- saturou no limite"
                                             : "");

        if (aplicado == 0) {
            break;
        }
        comandos++;
    }

    if (comandos >= POSICAO_MAX_PASSOS) {
        printk("  ⚠️ teto de %d comandos atingido SEM a camada saturar - omega\n"
               "     esta subestimado, e o limite de curso nao esta protegendo\n",
               POSICAO_MAX_PASSOS);
    }

    led(LED_APAGADO);
    k_msleep(POSICAO_PAUSA_MS);
    return comandos;
}

/* Varre 0 -> limite num sentido -> volta -> limite no outro -> volta.

   ⚠️ "Voltar" aqui e comandar o sentido oposto ate a camada recusar, nao buscar
   uma posicao: NAO HA ENCODER. Se o omega estiver errado, o eixo nao fecha no
   zero fisico de onde partiu - e essa diferenca visivel E o erro acumulado da
   odometria. E o resultado mais valioso deste teste. */
static void medir_posicao(const char *eixo,
                          uint32_t (*mover)(servo_dir_t, uint32_t),
                          int32_t (*theta_d10)(void),
                          void (*zerar)(void))
{
    printk("\n=== MEDICAO DE POSICAO: eixo %s ===\n", eixo);
    printk("Origem = onde o eixo esta AGORA (posicione a mao antes de ligar).\n");
    printk("LED ACESO = girando. LED APAGADO = parado.\n");
    printk("⭐ MARQUE a posicao de partida: e contra ela que se mede o erro.\n");

    zerar();

    int ida = varrer(eixo, "sentido horario", SERVO_HORARIO, mover, theta_d10);
    printk("[posicao] %s: %d comandos ate o limite; voltando\n", eixo, ida);
    varrer(eixo, "anti-horario (voltando)", SERVO_ANTIHORARIO, mover, theta_d10);

    printk("[posicao] %s: agora o outro lado\n", eixo);
    varrer(eixo, "anti-horario", SERVO_ANTIHORARIO, mover, theta_d10);
    varrer(eixo, "horario (voltando)", SERVO_HORARIO, mover, theta_d10);

    printk("[posicao] %s: fim. theta = %+d decimos de grau\n",
           eixo, theta_d10());
    printk("⚠️ O eixo voltou ao ponto marcado? A diferenca e o erro acumulado\n"
           "da odometria - e a medida de quanto omega ainda precisa ser medido.\n");
}

void teste_medicao_posicao_vertical(void)
{
    medir_posicao("vertical", servo_vertical_direcao_tempo,
                  servo_theta_vertical_d10, servo_zerar_vertical);
}

void teste_medicao_posicao_horizontal(void)
{
    medir_posicao("horizontal", servo_horizontal_direcao_tempo,
                  servo_theta_horizontal_d10, servo_zerar_horizontal);
}
