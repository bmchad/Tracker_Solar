#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <pwm_z42.h>
#include <teste_console.h>

int teste_console(int tentativas){
    if(tentativas == 0) for(int n=0;;n++){
        printk("Tentativa %d\n", n);
        k_msleep(500);
    }
    else if(tentativas < 0) return 1;
    else if(tentativas > 0) for(int i=0; i<tentativas;i++){
        printk("Tentativa %d\n", i);
        k_msleep(500);
    }
    return 0;
}