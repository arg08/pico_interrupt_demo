// Test harness for Mode7 display output.

// Compile option to generate sync pulses (standalone display),
// otherwise the sync pin is an input and the Electron assumed to generate them
#define	GENERATE_SYNCS	1




#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

// This gets the git revision number and state into the binary
#include "git.h"

// Bitmask with 1 bit set in it corresponding to the timer channel
// that we have claimed - used for timer_hw->intf
static volatile uint32_t timer_bitmask = 0;


// This interrupt handler runs on core1 to handle timer interrupts
// forced by user input on core0
static void __not_in_flash_func(timer_irq_handler)(void)
{
	// Prevent the handler getting immediately re-entered by disabling
	// the interrupt in the peripheral.  Remains enabled in (our core's) NVIC
	// so the other core can trigger it again later.
	hw_clear_bits(&timer_hw->intf, timer_bitmask);

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
	int timer_no;
	// Setup for interrupts via the Timer interrupt-force facility.
	// Could hard-wire timer 0, 1 or 2, but the system by default is
	// already using Timer3.  This version more politely claims a spare
	// timer channel, at the expense of having to keep the bit number
	// in a variable.

	timer_no = hardware_alarm_claim_unused(false);
	if (timer_no < 0) printf("No timer hardware available\n");
	else
	{
		printf("Setting up to use timer %u\n", timer_no);
		timer_bitmask = 1<<timer_no;
		// Set up our handler for it, on this core, and enable the interrupt
		// in the NVIC.  Don't need to enable it in the peripheral (ever),
		// as timer_hw->intf overrides timer_hw->inte.
		irq_set_exclusive_handler(TIMER_IRQ_0 + timer_no, timer_irq_handler);
		irq_set_enabled(TIMER_IRQ_0 + timer_no, true);
	}

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

static bool core1_launched = false;

// Would normally just call multicore_launch_core1() at the top of main,
// but this version delays starting it so we have time to connect the
// USB console and see debug messages as it starts up.
static void ensure_core1_launched(void)
{
	if (!core1_launched)
	{
		multicore_launch_core1(core1_main_loop);
		printf("Launching core1\n");
		core1_launched = true;
	}
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

	// Discard any character that got in the UART during powerup
	getchar_timeout_us(10);

	for (;;)
	{
		int c = getchar_timeout_us(100);
		if (c >= 0)
		{
			switch (c)
			{
				case 'c':
					printf("hardware_alarm_claim_unused() returns %d\n",
						hardware_alarm_claim_unused(false));
					break;
				case 't':
				case 'T':
					ensure_core1_launched();
					printf("Forcing timer interrupt\n");
					hw_set_bits(&timer_hw->intf, timer_bitmask);
					break;

				case 'F':
				case 'f':
					ensure_core1_launched();
					printf("Writing word to FIFO\n");
					multicore_fifo_push_blocking(0x1234);
					break;

				case 'L':
				case 'l':
					if (core1_launched) printf("Core1 already running\n");
					else ensure_core1_launched();
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
