/**
 * @file rc_test_buttons.c
 * @example    rc_test_buttons
 *
 * This is a very basic test of button functionality. It simply prints to the
 * screen when a button has been pressed or released.
 **/
#include <stdio.h>
#include <signal.h>
#include <rc/button.h>
#include <rc/time.h>
#include <rc/gpio.h>
#include <rc/adc.h>
#include <math.h> 


int main()
{ 
    if(rc_adc_init() == -1){
            fprintf(stderr,"ERROR: failed to run rc_init_adc()\n");
            return -1;
    }

    int output_init = rc_gpio_init (1, 18, GPIOHANDLE_REQUEST_INPUT);
    printf("The output of init is %d \n", output_init);


    while(1) {
        int16_t adc_read= rc_adc_read_raw(0);

        int R0 = 100000;
        float R = 4095.0/adc_read-1.0;
        printf("The R is %f \n", R);

        int B = 4275;
        
        R = R0 * R;
        float temperature=1.0/(log(R/R0)/B+1/298.15)-273.15;
        printf("The temperature is %f", temperature);

        printf("The adc out %d \n", adc_read);
        printf("The output is %d \n", rc_gpio_get_value(1,18));
    }
}
