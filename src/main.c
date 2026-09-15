/* NOTAS
pwm do horizontal = ptd3
pwm do vertical = ptd0
ldr superior esquerdo no ptb0
ldr inferior esquerdo no ptb1
ldr superior direito no ptb2
ldr inferior direito no ptb3
os quatro ldr sao ptb0..ptb3 = A0..A3, contiguos no header arduino
em init_hora.c coloque hora, minuto segundo

ANTES DE LIGAR, POSICIONE MANUALMENTE O TRACKER COM A PLACA
PARALELA AO SOLO E ALINHADA COM O EQUADOR

⚠️ A linha antiga "em angulo_incidencia_solar.c preencha dia mes ano latitude"
saiu: com a DEC-13 o eixo horizontal ficou em malha fechada pura e calcular_theta()
deixou de ser chamado. Ver o comentario no fim deste arquivo.

*/

#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <servo_motor.h>
#include <teste_ldr.h>
#include <pisca_led.h>
#include <teste_rot_cont.h>
#include <teste_console.h>

// Para um clock de 48 MHz e prescaler de 128 (PS_128), a frequência do timer é 375 kHz.
// Para um período de 20ms (50Hz), TPM_MODULE (MOD) deve ser 7500.
// 1ms (0 graus) = 375 contagens.
// 2ms (180 graus) = 750 contagens.
#define TPM_MODULE 7500

/* --- Constantes do laço de tracking ---------------------------------------- */

#define AMOSTRAS          5   /* impar, para a mediana nao precisar de media   */

#define MARGEM_MV        60   /* diferenca abaixo disto e ruido/nuvem         */

#define PASSO_MS        200   /* passo fixo. graus = PASSO_MS * omega / 1000, */
                              /* ou ~12 graus com omega = 60 graus/s          */

/* ⛔ Passo abaixo do minimo da camada faria o tracker NUNCA mover, e em
   silencio: mover() recusaria todo comando e devolveria 0 para sempre. Pega em
   tempo de compilacao, que e onde este erro custa menos. */
BUILD_ASSERT(PASSO_MS >= SERVO_MS_MIN,
             "PASSO_MS abaixo de SERVO_MS_MIN: todo comando seria recusado");

#define PERIODO_MS     1000   /* entre amostras, com o eixo parado            */
#define ASSENTAMENTO_MS 300   /* depois de mover, antes de reler (DEC-11)     */
#define NOITE_MS       5000

/* ⭐ A MATRIZ DE MISTURA: o unico lugar do firmware que conhece a geometria
   optica. Arranjo em CRUZ (`+`), entao cada quadrante e uma DIAGONAL e cada
   eixo sai de uma SOMA DE PARES - e a "comparacao de pares" do controle, que
   deixou de ser entre dois sensores e passou a ser entre dois LADOS.

   Se a aleta voltar a ser um "X", cada sensor volta a apontar para uma direcao
   e ISTO e a unica coisa que muda:
       *erro_h = mv[ESQ]  - mv[DIR];
       *erro_v = mv[CIMA] - mv[BAIXO];

   O SINAL depende da topologia do divisor, e ela esta fechada (P-03, confirmada
   na montagem em 2026-09-15): pull-down, 10 kOhm para GND e LDR para 3,3 V,
   logo mais luz => mais tensao, e as duas contas valem como estao escritas.
   Se a montagem virar pull-up, inverta as duas. */
static void erros(const int32_t mv[LDR_COUNT], int32_t *erro_h, int32_t *erro_v)
{
    *erro_h = (mv[LDR_SUP_ESQ] + mv[LDR_INF_ESQ])
            - (mv[LDR_SUP_DIR] + mv[LDR_INF_DIR]);

    *erro_v = (mv[LDR_SUP_ESQ] + mv[LDR_SUP_DIR])
            - (mv[LDR_INF_ESQ] + mv[LDR_INF_DIR]);
}

/* Mediana de AMOSTRAS valores, por ordenacao por insercao numa copia.

   ⭐ Substitui o filtro de votacao quadratica (D-10), que comparava cada
   amostra da esquerda com cada amostra da direita num produto cruzado 5x5.
   Aquilo misturava a diferenca ESPACIAL (o que se quer medir) com a variacao
   TEMPORAL de cada sensor (o que se quer filtrar); disparava com UM outlier,
   apesar de imprimir "certeza absoluta"; e os dois contadores podiam passar do
   limiar ao mesmo tempo, caso em que o `else if` escolhia um lado em silencio.

   Com o erro ja misturado antes de entrar no buffer, as cinco amostras sao da
   MESMA grandeza e a comparacao passa a ser so temporal. A mediana descarta o
   outlier sem arrastar o resultado para o lado dele - que e o que uma media
   faria - e devolve MAGNITUDE, que e o que o passo proporcional precisa. */
static int32_t mediana(const int32_t v[AMOSTRAS])
{
    int32_t c[AMOSTRAS];

    for (int i = 0; i < AMOSTRAS; i++) {
        c[i] = v[i];
    }

    for (int i = 1; i < AMOSTRAS; i++) {
        int32_t x = c[i];
        int j = i - 1;

        while (j >= 0 && c[j] > x) {
            c[j + 1] = c[j];
            j--;
        }
        c[j + 1] = x;
    }

    return c[AMOSTRAS / 2];
}

/* Decide o SENTIDO a partir do erro e anda o passo fixo. Devolve 1 se o eixo de
   fato andou.

   ⭐ Tres vias e nenhuma aritmetica: passou da margem para um lado, passou para
   o outro, ou fica parado. E o desenho do codigo original - o que mudou e que a
   unidade do passo virou TEMPO, porque o servo e de rotacao continua e o
   comando real e duracao, nao angulo.

   ⚠️ CONVENCAO DE SENTIDO: erro positivo significa "mais luz do lado
   esquerdo/de cima", e o painel tem de girar PARA LA - aqui isso e
   SERVO_ANTIHORARIO. Se na bancada um eixo FUGIR da luz em vez de segui-la,
   quem corrige e a montagem daquele eixo: troque `+` por `-` no cnv_horario /
   cnv_anti dele em sevo_motor.c, como o comentario do eixo_t ja prevê. Corrigir
   aqui inverteria tambem o sinal de theta, e a odometria passaria a contar ao
   contrario. */
static int corrigir(const char *eixo, int32_t erro,
                    uint32_t (*mover)(servo_dir_t, uint32_t))
{
    servo_dir_t dir;

    if (erro > MARGEM_MV) {
        dir = SERVO_ANTIHORARIO;
    } else if (erro < -MARGEM_MV) {
        dir = SERVO_HORARIO;
    } else {
        return 0;               /* dentro da margem: nao mexe */
    }

    uint32_t aplicado = mover(dir, PASSO_MS);

    printk("[track] %s: erro=%+d mV -> %s (aplicado %u ms)\n",
           eixo, (int)erro,
           (dir == SERVO_HORARIO) ? "horario" : "anti-horario",
           (unsigned)aplicado);

    return aplicado > 0;
}

//função principal
int main(void){

    /* ⭐ RECIBO DE GRAVACAO, e e a PRIMEIRA coisa que main() faz de proposito:
       acende o LED na cor desta build e deixa aceso. Se a placa nao mudar de
       cor depois de gravar, o upload NAO chegou ao alvo - troque
       COR_DESTA_GRAVACAO em pisca_led.h a cada build. */
    led(COR_DESTA_GRAVACAO);

    teste_console(10);   /* a serial vive? 0,5 s cada; n = 0 e infinito */
    // Configura os motores
    pwm_tpm_Init(TPM0, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM0, 3, TPM_PWM_H, GPIOD, 3);
    pwm_tpm_Ch_Init(TPM0, 0, TPM_PWM_H, GPIOD, 0);

    //configura o ldr
    printk("Iniciando Aplicacao de Monitoramento do LDR...\n");

    // Inicializa o hardware do ADC

    if (ldr_inicializar() < 0)  //configura o ldr
    {
        printk("Falha critica na inicializacao do ADC. Parando.\n");
        return -1;
    }

    printk("ADC pronto. Entrando no loop de leitura.\n");

    // Inicia o relógio usando os valores que o usuário digitou no init_hora.c
    relogio_init();

    /* ⚠️ TESTES DE BANCADA. Todos RETORNAM agora, entao podem ser encadeados -
       mas cada um custa tempo de boot, e o custo esta anotado. Comente os que
       nao estiver usando.

       ⚠️ `n = 0` e INFINITO nos quatro, nao "nenhuma repeticao".

       Ordem util quando se quer medir tudo: console (a serial vive?) -> omega
       (a odometria depende dele) -> posicao -> LDR. */

    teste_rot_cont(5);               /* o servo gira?          4 s cada  =  20 s */
    teste_rot_cont_medir_omega(1);   /* quanto vale omega?    52 s cada  =  52 s */
    teste_medicao_posicao_vertical();     /* odometria e limite, pela camada: 8 s */
    teste_medicao_posicao_horizontal();   /*                                 14 s */
    teste_ldr(10);                   /* 4 quadrantes, soma de pares    0,5 s cada */

    /* ⭐ ORIGEM DA ODOMETRIA, e ela vem DEPOIS dos testes de proposito.

       ⛔ Zerar antes deles era um BUG: teste_rot_cont() e medir_omega() giram
       por CnV cru, centenas de graus, sem tocar em theta; e os testes de posicao
       terminam com theta NO EXTREMO (+1200 no horizontal, +500 no vertical).
       O laco comecava entao acreditando estar no fim de curso, e ms_que_cabem()
       devolvia 0 para um dos sentidos - o tracker recusava metade das correcoes
       desde o primeiro ciclo, no meio do curso fisico real.

       ⚠️ A origem precisa ser o CENTRO do curso util, nao um ponto qualquer: a
       janela de limite e simetrica, entao zerar fora do meio faz um dos lados
       nao caber. O bloco de NOTAS no topo manda posicionar o tracker a mao antes
       de ligar - se algum teste acima girou os eixos, reposicione A MAO antes de
       confiar no laco. */
    servo_zerar_horizontal();
    servo_zerar_vertical();

    /* --- LACO DE TRACKING --------------------------------------------------

       Buffer circular de AMOSTRAS erros JA MISTURADOS, um vetor por eixo.

       ⚠️ Guardar o erro e nao as leituras cruas e o que fecha o D-10: cada
       posicao do vetor e uma medida da mesma grandeza, tirada no mesmo
       instante que as outras tres leituras daquele ciclo. O buffer antigo
       guardava sensor por sensor, e o filtro entao comparava leituras de
       instantes diferentes como se fossem posicoes diferentes. */
    int32_t hist_h[AMOSTRAS] = {0};
    int32_t hist_v[AMOSTRAS] = {0};
    int i = 0;
    int buffer_cheio = 0;

    while (1) {
        relogio_atualizar(); // Mantém o relógio atualizando hora_atual, etc.

        // Lógica de Dia e Noite
        if (hora_atual >= 18 || hora_atual < 6) {
            printk("Fim de tarde! Painel indo dormir (Hora: %02d:%02d)...\n",
                   hora_atual, minuto_atual);
            led(LED_APAGADO);

            /* ⚠️ Zera o historico junto: sem isto, o amanhecer decidiria com
               cinco amostras do anoitecer - o sol do outro lado do ceu. */
            i = 0;
            buffer_cheio = 0;

            k_msleep(NOITE_MS);
            continue;
        }

        /* 1. LEITURA - uma chamada, quatro conversoes encostadas.

           D-01: erro e valor viajam por canais separados, e falha descarta o
           CICLO INTEIRO. Antes uma falha devolvia -1.0, entrava no historico e
           VOTAVA: o firmware concluia que o sol estava do lado oposto e movia a
           placa por causa de um erro de barramento. */
        int32_t mv[LDR_COUNT];

        if (ldr_ler_todos(mv) < 0) {
            led(LED_VERMELHO);
            k_msleep(PERIODO_MS);
            continue;
        }

        /* 2 e 3. ERRO POR SOMA DE PARES, direto para dentro do buffer. */
        erros(mv, &hist_h[i], &hist_v[i]);

        i++;
        if (i == AMOSTRAS) {
            i = 0;
            buffer_cheio = 1;
        }

        if (!buffer_cheio) {
            k_msleep(PERIODO_MS);
            continue;           /* ainda enchendo: nao ha o que decidir */
        }

        /* 4. DECISAO - uma mediana por eixo, e os dois sao independentes por
           construcao. Nao existe mais o caso "as duas direcoes ganharam". */
        int32_t erro_h = mediana(hist_h);
        int32_t erro_v = mediana(hist_v);

        /* 5. MOVIMENTO - os dois eixos a partir do MESMO instantaneo.

           ⚠️ Girar em azimute mexe um pouco no erro vertical, entao o erro_v
           usado aqui fica levemente velho depois do movimento horizontal. Nao
           se corrige: o ciclo seguinte le de novo, e a malha absorve. Reler
           entre os dois eixos custaria uma amostra tirada com o servo ainda
           assentando, que e pior (DEC-11). */
        int moveu = 0;

        moveu |= corrigir("horizontal", erro_h, servo_horizontal_direcao_tempo);
        moveu |= corrigir("vertical",   erro_v, servo_vertical_direcao_tempo);

        /* 6. CADENCIA */
        if (moveu) {
            led(COR_DESTA_GRAVACAO);

            /* ⭐ O historico descreve a posicao ANTIGA. Mantê-lo faria a
               proxima decisao misturar cinco amostras de antes do movimento com
               uma de depois - e a mediana de seis instantes em duas posicoes
               diferentes nao mede nada. */
            i = 0;
            buffer_cheio = 0;

            k_msleep(ASSENTAMENTO_MS);
        } else {
            led(LED_VERDE);     /* no alvo: erro abaixo do passo minimo */

            printk("[track] no alvo: erro_h=%+d erro_v=%+d mV | "
                   "theta_h=%+d theta_v=%+d decimos de grau\n",
                   (int)erro_h, (int)erro_v,
                   (int)servo_theta_horizontal_d10(),
                   (int)servo_theta_vertical_d10());

            k_msleep(PERIODO_MS);
        }
    }

    /* ⚠️ DIVIDA REGISTRADA: lib/angulo_incidência_solar/ ficou SEM CHAMADOR.

       calcular_theta(HL, gama_p) espera o azimute do painel, e com a DEC-13 o
       eixo horizontal ficou em malha fechada pura - nao ha mapeamento de
       azimute, logo nao ha gama_p para passar. Chamá-la com o theta RELATIVO da
       odometria produziria um numero errado, e logar numero errado e pior que
       nao logar. A biblioteca continua na arvore; o include dela saiu deste
       arquivo pela regra do D-34 (incluir so o que se usa).

       E o D-11 fecha aqui: theta era usado como POSICAO a comandar no vertical,
       quando e funcao de custo - e com rotacao continua nem existe posicao para
       mandar. */

    return 0;
}
