#ifndef TESTES_H
#define TESTES_H

/* D-02: estas rotinas NAO inicializam hardware.
   Elas recebem TPM0 e o ADC ja prontos, porque quem inicializa e o main().
   Antes cada uma repetia as mesmas cinco chamadas de pwm_tpm_Init /
   pwm_tpm_Ch_Init / ldr_inicializar, e todo boot configurava o hardware tres
   vezes. Nao era so redundancia: pwm_tpm_Init faz `tpm->SC |= ...`, um OR e
   nao uma escrita, entao reinicializar com parametros DIFERENTES acumula bits
   em vez de substituir. Passava por sorte porque os tres lugares pediam os
   mesmos valores.

   D-06: nao chame isto no caminho normal de boot. Ver RODAR_TESTES_NO_BOOT
   no platformio.ini. */

int teste_motores(void);

int teste_ldr(void);

#endif /* TESTES_H */
