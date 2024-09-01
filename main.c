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
#include "usb_descriptors.h"

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
static void cdc_task(void);
static void hid_task(void);

typedef enum {
  MIDI_A = 0,
  MIDI_B = 1,
  MIDI_C = 2,
  MIDI_D = 3,
  NUM_PHY_MIDI_PORT_PAIRS
} LM_MIDI_PORT;

static void* midi_uarts[NUM_PHY_MIDI_PORT_PAIRS]; // MIDI IN A-D and MIDI OUT A-D

static const size_t MIDI_TX_GPIO[NUM_PHY_MIDI_PORT_PAIRS]   = { 24, 25, 22, 23};
static const size_t MIDI_RX_GPIO[NUM_PHY_MIDI_PORT_PAIRS]   = { 11, 10,  9,  8};
static const size_t MIDI_TXEN_GPIO[NUM_PHY_MIDI_PORT_PAIRS] = { 20, 19, 18, 21};
/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  // Set up MIDI TX EN for all 4 TX ports (needed for V1 HW)
  for(size_t n = 0; n <NUM_PHY_MIDI_PORT_PAIRS; n++) {
    gpio_init(MIDI_TXEN_GPIO[n]);
    gpio_set_dir(MIDI_TXEN_GPIO[n], true);
    gpio_put(MIDI_TXEN_GPIO[n], 1);
  }
  // Create the MIDI UARTs
  for(size_t n = 0; n < NUM_PHY_MIDI_PORT_PAIRS; n++) {
    midi_uarts[n] = pio_midi_uart_create(MIDI_TX_GPIO[n], MIDI_RX_GPIO[n]);
    if(midi_uarts[n] == 0) {
        printf("Error creating UART %zu\r\n", n);
    }
  }
  printf("Lenkaudio MIDIstributor V1\r\n");

  while (1)
  {
    tud_task(); // tinyusb device task
    midi_task();
    led_blinking_task();
    cdc_task(); //LK: ToDo
    hid_task(); //LK: ToDo
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
    /* LK: ToDo: dynamic routing, currently static HW IN N -> USB IN N */
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
    if (!connected) {
        return;
    }
    uint8_t rx[48];
    uint8_t cable_num;
    uint8_t npushed = 0;
    uint32_t nread =  tud_midi_demux_stream_read(&cable_num, rx, sizeof(rx));
    while (nread > 0) {
        if (cable_num < NUM_PHY_MIDI_PORT_PAIRS) {
            /* LK: ToDo: dynamic routing, currently static USB OUT N -> HW OUT N */
            npushed = pio_midi_uart_write_tx_buffer(midi_uarts[cable_num], rx, nread);
        }
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


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
/* LK: ToDo: Console */
void cdc_task(void) {
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if (tud_cdc_available()) {
      // read data
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void) count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if (dtr) {
    // Terminal connected
  } else {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
  (void) itf;
}


//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
/* LK: ToDo: HID messages routing */

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      if ( btn )
      {
        uint8_t keycode[6] = { 0 };
        keycode[0] = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        has_keyboard_key = true;
      }else
      {
        // send empty key report if previously has key pressed
        if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        has_keyboard_key = false;
      }
    }
    break;

    case REPORT_ID_MOUSE:
    {
      int8_t const delta = 5;

      // no button, right + down, no scroll, no pan
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);
    }
    break;

    case REPORT_ID_CONSUMER_CONTROL:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if ( btn )
      {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      }else
      {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        has_consumer_key = false;
      }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
      {
        .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
        .hat = 0, .buttons = 0
      };

      if ( btn )
      {
        report.hat = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        has_gamepad_key = true;
      }else
      {
        report.hat = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
      }
    }
    break;

    default: break;
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_KEYBOARD, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}