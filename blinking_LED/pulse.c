/*
   pulse.c

   gcc -o pulse pulse.c -lpigpio -lrt -lpthread

   sudo ./pulse
*/

#include <stdio.h>
#include <pigpio.h>

int main(int argc, char *argv[])
{
    double start;

    if (gpioInitialise() < 0)
    {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }

    // Set GPIO mode
    gpioSetMode(6, PI_OUTPUT);

    uint8_t k = 0;
    uint8_t flag = 0;
    start = time_time();
    while ((time_time() - start) < 60.0) //last for max 60 sec
        {

        //alternate flag at dutycycle 100% and 0%
        if (k == 255) {
            flag = 1;
        } else if (k == 0) {
            flag = 0;
        }

        gpioPWM(6, k); // set dutycycle
        time_sleep(0.001);

        //change dutycycle depending on flag
        if (flag == 0) {
            k++;
        } else {
            k--;
        }
    }

    // Stop DMA, release resources
    gpioTerminate();

    return 0;
}
