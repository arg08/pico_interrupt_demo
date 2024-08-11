// Test harness for Mode7 display output.

// Compile option to generate sync pulses (standalone display),
// otherwise the sync pin is an input and the Electron assumed to generate them
#define	GENERATE_SYNCS	1




#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

// This gets the git revision number and state into the binary
#include "git.h"


// This interrupt handler runs on core1 to handle timer interrupts
// forced by user input on core0
static void __not_in_flash_func(timer_irq_handler)(void)
{
	// Prevent the handler getting immediately re-entered by disabling
	// the interrupt in the peripheral.  Remains enabled in (our core's) NVIC
	// so the other core can trigger it again later.
	hw_clear_bits(&timer_hw->inte, (1<<3));

	printf("Timer interrupt occurred in core %u\n", get_core_num());
}

// This interrupt handler runs on core1 to handle words arriving
// on the inter-core FIFO
// forced by user input on core0
static void __not_in_flash_func(fifo_irq_handler)(void)
{
	printf("FIFO interrupt occurred in core %u\n", get_core_num());
	if (multicore_fifo_rvalid())
	{
		uint32_t val = multicore_fifo_pop_blocking();
		printf("FIFO data interrupt, word is %08lx\n", val);

	}
	else
	{
		printf("FIFO interrupt for over/underflow errors - clearing status\n");
		multicore_fifo_clear_irq();
	}
}

static void core1_main_loop(void)
{
	// Setup for interrupts via the Timer3 interrupt-force facility.

	// We just force the interrupt once at initialisation, and then
	// enable/disable it to generate/acknowledge the interrupt.
	// Interrupt remains pending in the peripheral, but we switch the
	// enable towards the NVIC
	hw_set_bits(&timer_hw->intf, (1<<3));
	// Now set up our handler for it, on this core.
#if 0
// XXX This crashes for unknown reason
	irq_set_exclusive_handler(TIMER_IRQ_3, timer_irq_handler);
#endif
	irq_set_enabled(TIMER_IRQ_3, true);

	// ----------------------------------------------------------------------
	// Setup for interrupts via the inter-core FIFO facility
	irq_set_exclusive_handler(SIO_IRQ_PROC1, fifo_irq_handler);
	irq_set_enabled(SIO_IRQ_PROC1, true); 


	for (;;)
	{
		busy_wait_ms(5000);
		printf("Core1 still alive\n");
	}
}

int pollchar(void)
{
  int c = getchar_timeout_us(0);
  return c;
}


// -----------------------------------------------------------------------------
int main(void)
{
	unsigned offset;
	bool launched = false;
	bool clock_ok;

    watchdog_enable(5000, true);    // Enable watchdog with 5sec timeout

	// USB console for monitoring
	stdio_init_all();

	// Start up the other core.
	multicore_launch_core1(core1_main_loop);

	// Discard any character that got in the UART during powerup
	getchar_timeout_us(10);

	for (;;)
	{
		int c = getchar_timeout_us(100);
		if (c >= 0)
		{
			switch (c)
			{
				case 't':
				case 'T':
					printf("Enabling timer3 interrupt\n");
					hw_set_bits(&timer_hw->inte, (1<<3));
					break;

				case 'F':
				case 'f':
					printf("Writing word to FIFO\n");
					multicore_fifo_push_blocking(0x1234);
					break;

				case 'b':
				case 'B':
					printf("Exiting to bootrom\n");
					busy_wait_ms(100);
					// 1st parameter is LED to use for mass storage activity
					// 2nd param enables both USB mass storage and PICOBOOT
					reset_usb_boot(1 << PICO_DEFAULT_LED_PIN, 0);
					break;

				case 'v':
				default:
					printf("Pi Pico inter-core IRQ demo - %s\n%s%s\n",
						git_Describe(),
						git_CommitDate(),
						git_AnyUncommittedChanges() ? " ***modified***" : "");
					printf("Presss 'F' for FIFO, 'T' for timer\n");
					break;
			}
		}

		// Feed the watchdog
		watchdog_update();
    }
	// Never exits
}
