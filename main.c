/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 rppicomidi
 * Copyright (c) 2024 Lena Kryger (lenkaud.io)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "pio_midi_uart_lib.h"
#include "midi_device_multistream.h"
//--------------------------------------------------------------------+
// This program routes 5-pin DIN MIDI IN signals A & B to USB MIDI
// virtual cables 0 & 1 on the USB MIDI Bulk IN endpoint. It also
// routes MIDI data from USB MIDI virtual cables 0-5 on the USB MIDI
// Bulk OUT endpoint to the 5-pin DIN MIDI OUT signals A-F.
// The Pico board's LED blinks in a pattern depending on the Pico's
// USB connection state (See below).
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

static void led_blinking_task(void);
static void midi_task(void);

typedef enum {
  MIDI_A = 0,
  MIDI_B = 1,
  MIDI_C = 2,
  MIDI_D = 3,
  NUM_PHY_MIDI_PORT_PAIRS
} LM_MIDI_PORT;

static void* midi_uarts[NUM_PHY_MIDI_PORT_PAIRS]; // MIDI IN A-D and MIDI OUT A-D
//static void* midi_outs[4];  // MIDI OUT C-F


// MIDI UART pin usage (Move them if you want to)
//static const uint MIDI_OUT_GPIO[NUM_PHY_MIDI_PORT_PAIRS]
//static const uint MIDI_IN_GPIO[NUM_PHY_MIDI_PORT_PAIRS]
//static const uint MIDI_IN_A_GPIO = 5;
//static const uint MIDI_OUT_B_GPIO = 6;
//static const uint MIDI_IN_B_GPIO = 7;
//static const uint MIDI_OUT_C_GPIO = 10;
//static const uint MIDI_OUT_D_GPIO = 18;
//static const uint MIDI_OUT_E_GPIO = 3;
//static const uint MIDI_OUT_F_GPIO = 27;

static const size_t MIDI_TX_GPIO[NUM_PHY_MIDI_PORT_PAIRS]   = { 24, 25, 22, 23};
static const size_t MIDI_RX_GPIO[NUM_PHY_MIDI_PORT_PAIRS]   = {  8,  9, 10, 11};
static const size_t MIDI_TXEN_GPIO[NUM_PHY_MIDI_PORT_PAIRS] = { 20, 19, 18, 21};
/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  // Create the MIDI UARTs
  for(size_t n = 0; n < NUM_PHY_MIDI_PORT_PAIRS; n++) {
    midi_uarts[n] = pio_midi_uart_create(MIDI_TX_GPIO[n], MIDI_RX_GPIO[n]);
  }

  // Set up MIDI TX EN for all 4 TX ports
  uint txen_mask = 0u;
  for(size_t n = 0; n < NUM_PHY_MIDI_PORT_PAIRS; n++) {
    txen_mask |= 1 << MIDI_TXEN_GPIO[n];
  }
  gpio_init_mask(txen_mask);
  gpio_set_dir_out_masked(txen_mask);
  // Currently set TXEN to HIGH for all 4 ports
  gpio_set_mask(txen_mask);

  printf("4-IN 4-OUT USB MIDI Device adapter\r\n");
  // 
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+
static void poll_midi_uarts_rx(bool connected)
{
    uint8_t rx[48];
    // Pull any bytes received on the MIDI UART out of the receive buffer and
    // send them out via USB MIDI on virtual cable 0
    for (uint8_t cable = 0; cable < NUM_PHY_MIDI_PORT_PAIRS; cable++) {
        uint8_t nread = pio_midi_uart_poll_rx_buffer(midi_uarts[cable], rx, sizeof(rx));
        if (nread > 0 && connected)
        {
            uint32_t nwritten = tud_midi_stream_write(cable, rx, nread);
            if (nwritten != nread) {
                TU_LOG1("Warning: Dropped %lu bytes receiving from UART MIDI In %c\r\n", nread - nwritten, 'A'+cable);
            }
        }
    }
    
}

static void poll_usb_rx(bool connected)
{
    // device must be attached and have the endpoint ready to receive a message
    if (!connected)
    {
        return;
    }
    uint8_t rx[48];
    uint8_t cable_num;
    uint8_t npushed = 0;
    uint32_t nread =  tud_midi_demux_stream_read(&cable_num, rx, sizeof(rx));
    while (nread > 0) {
        if (cable_num < NUM_PHY_MIDI_PORT_PAIRS) {
            // then it is MIDI OUT A-D
            npushed = pio_midi_uart_write_tx_buffer(midi_uarts[cable_num], rx, nread);
        }
        //else if (cable_num < 6) {
        //    // then it is MIDI OUT C, D, E or F
        //    npushed = pio_midi_out_write_tx_buffer(midi_outs[cable_num-2], rx, nread);
        //}
        else {
            TU_LOG1("Received a MIDI packet on cable %u", cable_num);
            npushed = 0;
            continue;
        }
        if (npushed != nread) {
            TU_LOG1("Warning: Dropped %lu bytes sending to MIDI Out Port %c\r\n", nread - npushed, 'A' + cable_num);
        }
        nread =  tud_midi_demux_stream_read(&cable_num, rx, sizeof(rx));
    }
}

static void drain_serial_port_tx_buffers()
{
    uint8_t cable;
    for (cable = 0; cable < NUM_PHY_MIDI_PORT_PAIRS; cable++) {
        pio_midi_uart_drain_tx_buffer(midi_uarts[cable]);
    }
    //for (cable = 2; cable < 6; cable++) {
    //    pio_midi_out_drain_tx_buffer(midi_outs[cable-2]);
    //}
}
static void midi_task(void)
{
    bool connected = tud_midi_mounted();
    poll_midi_uarts_rx(connected);
    poll_usb_rx(connected);
    drain_serial_port_tx_buffers();
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
