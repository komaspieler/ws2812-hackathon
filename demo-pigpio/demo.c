#include <stdio.h>
#include <pigpio.h>
#include <stdint.h>
#include <time.h>

// Change this to be at least as long as your pixel string (too long will work fine, just be a little slower)

#define PIXELS 330  // Number of pixels in the string

#define RPI_GPIO_BCM_PIN 6 // our LED strip data-in is connected to this BCM pin

// These are the timing constraints taken mostly from the WS2812 datasheets 
// These are chosen to be conservative and avoid problems rather than for maximum throughput 

#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

#define RES 6000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

#define NS_PER_SEC (1000000000L)        // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_SEC (600000000UL)    // fixed to 600 MHz currently (see /etc/rc.local for power save policy)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// Actually send a bit to the string. We must to drop to asm to enusre that the complier does
// not reorder things and make it so the delay happens in the wrong place. 

inline void sendBit(uint8_t bitVal) {
    int i;
    struct timespec tWait;
    tWait.tv_sec = 0;
    clock_t meas1,meas2;

    if (bitVal) {				// 0 bit
        tWait.tv_nsec = T1H;
        //~ meas1 = clock();
        gpioWrite(RPI_GPIO_BCM_PIN, 1); // set output bit
        //~ meas2 = clock();
        //~ printf("%d\n", (meas2-meas1));
		asm volatile (
			"mov r2, #1000\n\t"
			"L3:\n\t"
			"subs r2, r2, #1\n\t"
			"bne L3"
		);
        gpioWrite(RPI_GPIO_BCM_PIN, 0); // clear output bit
		asm volatile (
			"mov r2, #2000\n\t"
			"L4:\n\t"
			"subs r2, r2, #1\n\t"
			"bne L4"
		);

        /*
		asm volatile (
			"sbi %[port], %[bit] \n\t"				// Set the output bit
			".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			"cbi %[port], %[bit] \n\t"                              // Clear the output bit
			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			::
			[port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
			[bit]		"I" (PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T1H) - 2),		// 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
			[offCycles] 	"I" (NS_TO_CYCLES(T1L) - 2)			// Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness

		);
        */
    } else {					// 1 bit

		// **************************************************************************
		// This line is really the only tight goldilocks timing in the whole program!
		// **************************************************************************

        gpioWrite(RPI_GPIO_BCM_PIN, 1); // set output bit
        //~ asm volatile("nop");
        //~ asm volatile("nop");
        //~ asm volatile (
			//~ "mov r2, #110\n\t"
			//~ "L:\n\t"
			//~ "subs r2, r2, #1\n\t"
			//~ "bne L"
		//~ );
        gpioWrite(RPI_GPIO_BCM_PIN, 0); // clear output bit
		asm volatile (
			"mov r2, #2000\n\t"
			"L2:\n\t"
			"subs r2, r2, #1\n\t"
			"bne L2"
		);
        /*
		asm volatile (
			".rept %[onCycles] \n\t"				// Now timing actually matters. The 0-bit must be long enough to be detected but not too long or it will be a 1-bit
			"nop \n\t"                                              // Execute NOPs to delay exactly the specified number of cycles
			".endr \n\t"
			".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
			"nop \n\t"
			".endr \n\t"
			::
			[port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
			[bit]		"I" (PIXEL_BIT),
			[onCycles]	"I" (NS_TO_CYCLES(T0H) - 2),
			[offCycles]	"I" (NS_TO_CYCLES(T0L) - 2)

		);
      */
    }
    
    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
    // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
    // This has thenice side effect of avoid glitches on very long strings becuase 

    
}  

  
inline void sendByte( uint8_t byte ) {
    uint8_t bit;
    for( bit = 0 ; bit < 8 ; bit++ ) {
      
      sendBit( (byte & 0x80) );                // Neopixel wants bit in highest-to-lowest order
                                                     // so send highest bit (bit #7 in an 8-bit byte since they start at 0)
      byte <<= 1;                                    // and then shift left so bit 6 moves into 7, 5 moves into 6, etc
      
    }           
} 

/*

  The following three functions are the public API:
  
  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.  
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame. 
  
*/


// Set the specified pin up as digital out

void ledsetup() {
  gpioSetMode(RPI_GPIO_BCM_PIN, PI_OUTPUT);
}

inline void sendPixel( uint8_t r, uint8_t g , uint8_t b )  {  
  
  sendByte(g);          // Neopixel wants colors in green then red then blue order
  sendByte(r);
  sendByte(b);
  
}


// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
	time_sleep( (RES / 1000000000UL) + (1/1000000UL));				// Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}


/*

  That is the whole API. What follows are some demo functions rewriten from the AdaFruit strandtest code...
  
  https://github.com/adafruit/Adafruit_NeoPixel/blob/master/examples/strandtest/strandtest.ino
  
  Note that we always turn off interrupts while we are sending pixels becuase an interupt
  could happen just when we were in the middle of somehting time sensitive.
  
  If we wanted to minimize the time interrupts were off, we could 
  instead could get away with only turning off interrupts just for the 
  very brief moment when we are actually sending a 0 bit (~1us), as 
  long as we were sure that the total time taken by any interrupts + 
  the time in our pixel generation code never exceeded the reset time 
  (5us).
  
*/


// Display a single color on the whole string

void showColor( uint8_t r , uint8_t g , uint8_t b ) {
  int p;
  for(p=0; p<PIXELS; p++ ) {
    sendPixel( r , g , b );
  }

  show();
  
}

void rainbowCycle(unsigned int frames , unsigned int frameAdvance, unsigned int pixelAdvance ) {
  
  // Hue is a number between 0 and 3*256 than defines a mix of r->g->b where
  // hue of 0 = Full red
  // hue of 128 = 1/2 red and 1/2 green
  // hue of 256 = Full Green
  // hue of 384 = 1/2 green and 1/2 blue
  // ...
  
  unsigned int firstPixelHue = 0;     // Color for the first pixel in the string
  unsigned int i,j,currentPixelHue;
  unsigned char phase, step;
  unsigned int ps = 0;
  for(j=0; j<frames; j++) {                                  
    
    currentPixelHue = firstPixelHue;
    
    
    for(i=0; i< PIXELS; i++) {
      
      if (currentPixelHue>=(3*256)) {                  // Normalize back down incase we incremented and overflowed
        currentPixelHue -= (3*256);
      }
            
      phase = currentPixelHue >> 8;
      step = currentPixelHue & 0xff;
                 
      switch (phase) {
        
        case 0: 
          sendPixel( ~step>>ps , step>>ps ,  0 );
          break;
          
        case 1: 
          sendPixel( 0 , ~step>>ps , step>>ps );
          break;

        case 2: 
          sendPixel(  step>>ps ,0 , ~step>>ps );
          break;
          
      }
      
      currentPixelHue+=pixelAdvance;                                      
      
                          
    } 
    
    show();
    //~ printf("frame %d\n", j);
    time_sleep(0.001);
    firstPixelHue += frameAdvance;
           
  }
}

#define THEATER_SPACING (PIXELS/15)

void theaterChase( unsigned char r , unsigned char g, unsigned char b, double wait ) {
  int j,q,i,step;
  
  for (j=0; j< 3 ; j++) {  
    for (q=0; q < THEATER_SPACING ; q++) {
      step=0;
      
      for (i=0; i < PIXELS ; i++) {
        if (step==q) {          
          sendPixel( r , g , b );
        } else {
          sendPixel( 0 , 0 , 0 );
        }

        step++;
        
        if (step==THEATER_SPACING) step =0;
      }
      
      show();
      time_sleep(wait);
    }
  }
}


void detonate( unsigned char r , unsigned char g , unsigned char b , unsigned int delay) {
	
  // Then we fade to black....
  int fade;
  for(fade=0; fade<255; fade++ ) {
    showColor( (r*fade)>>8 ,(b*fade)>>8 , (g*fade)>>8 );
    time_sleep(0.005);
  }
  
  for(fade=256; fade>1; fade-- ) {
    showColor( (r * fade) / 256 ,(g*fade) /256 , (b*fade)/256 );
    time_sleep(0.005);
  }
  showColor( 0 , 0 , 0 );
  
  
}

void policeLights(void) {
	int i;
	
	for (i=0; i<(PIXELS>>1); i++) {
		sendPixel(255, 0, 0);
	}
	
	for (i=0; i<(PIXELS>>1); i++) {
		sendPixel(0, 0, 255);
	}
	time_sleep(0.2);
	for (i=0; i<(PIXELS>>1); i++) {
		sendPixel(0, 0, 255);
	}
	
	for (i=0; i<(PIXELS>>1); i++) {
		sendPixel(255, 0, 0);
	}
}

void knightRider(uint8_t r, uint8_t g, uint8_t b) {
  int j,q,i,k,step,count;
	count = 1;
    for (q=0; q < PIXELS ; q++) {
        
        for (k = 0; k < count; k++) {
			sendPixel(0,0,0);
		}
		
          sendPixel( r>>3 , g>>3 , b>>3 );
          sendPixel( r , g , b );
          sendPixel( r , g , b );
          sendPixel( r , g , b );
          sendPixel( r>>3 , g>>3 , b>>3 );
		
		for(k = count+5; k < PIXELS; k++) {
          sendPixel( 0 , 0 , 0 );
		}
		
		count++;
		
      show();
      time_sleep(0.001);
    }
    count = PIXELS+1;
    for (q=0; q < PIXELS ; q++) {
        
        for (k = 0; k < count; k++) {
			sendPixel(0,0,0);
		}
		
          sendPixel( r>>3 , g>>3 , b>>3 );
          sendPixel( r , g , b );
          sendPixel( r , g , b );
          sendPixel( r , g , b );
          sendPixel( r>>3 , g>>3 , b>>3 );
		
		for(k = count+5; k < PIXELS; k++) {
          sendPixel( 0 , 0 , 0 );
		}
		
		count--;
		
      show();
      time_sleep(0.001);
    }
}


int main(int argc, char *argv[]) {
    if (gpioInitialise() < 0)
    {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }
    
    ledsetup();
    while(1) {
        //~ showColor(255, 255, 255);
        time_sleep(0.1);
        //~ showColor(32, 64, 64);
        //~ time_sleep(1);
        showColor(2, 2, 2);
        //~ time_sleep(1);
        //~ rainbowCycle(5000 , 20 , 15 );
        //~ // Send a theater pixel chase in...
		//~ theaterChase(32, 32, 32, 0.08); // White
		//~ knightRider(0,0,255);
		//~ knightRider(0,255,0);
		//~ knightRider(255,0,0);
		//~ policeLights();
		//~ theaterChase(127,   0,   0, 0.08); // Red
		//~ theaterChase(  0,   0, 127, 0.08); // Blue
		//~ detonate( 32 , 32 , 32 , 600);
		//~ detonate( 32 , 0 , 0 , 600);
		//~ detonate( 32 , 32 , 0 , 600);
		//~ detonate( 0 , 0 , 32 , 600);
		//~ showColor(0,0,0);
    }
    
    // Stop DMA, release resources
    gpioTerminate();
}
