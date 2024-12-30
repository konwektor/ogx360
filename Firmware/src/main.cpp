// Co4pyright 2021, Ryan Wendland, ogx360
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>

#include "main.h"
#include "usbd/usbd_xid.h"
#include "usbh/usbh_xinput.h"

uint8_t player_id;
XID_ usbd_xid;
usbd_controller_t usbd_c[MAX_GAMEPADS];

#ifdef BLUERETRO
bool usb_active = false;  //flag to track if slave has been pinged
#endif
void setup()
{
    #ifdef BLUERETRO
    UDCON |= (1 << DETACH); // detach USB at program start - avoid blocking USB port if wired gamepad is also connected
    #endif
    
    Serial1.begin(115200);

    pinMode(ARDUINO_LED_PIN, OUTPUT);
    pinMode(PLAYER_ID1_PIN, INPUT_PULLUP);
    pinMode(PLAYER_ID2_PIN, INPUT_PULLUP);
    digitalWrite(ARDUINO_LED_PIN, HIGH);
       
    memset(usbd_c, 0x00, sizeof(usbd_controller_t) * MAX_GAMEPADS);

    for (uint8_t i = 0; i < MAX_GAMEPADS; i++)
    {
        usbd_c[i].type = DUKE;
        usbd_c[i].duke.in.bLength = sizeof(usbd_duke_in_t);
        usbd_c[i].duke.out.bLength = sizeof(usbd_duke_out_t);

        usbd_c[i].sb.in.bLength = sizeof(usbd_sbattalion_in_t);
        usbd_c[i].sb.out.bLength = sizeof(usbd_sbattalion_out_t);
    }

    //00 = Player 1 (MASTER)
    //01 = Player 2 (SLAVE 1)
    //10 = Player 3 (SLAVE 2)
    //11 = Player 4 (SLAVE 3)
    player_id = digitalRead(PLAYER_ID1_PIN) << 1 | digitalRead(PLAYER_ID2_PIN);
    #ifdef BLUERETRO
    player_id++; // BlueRetro compatibility, make player 1 slave 1 instead of master.
    #endif
           
    if (player_id == 0)
    {
        master_init();
    }
    else
    {
        slave_init();
    }
}

void loop()
{
    #if (0)
    // Performance loop timing.
    static uint32_t loop_cnt = 0;
    static uint32_t loop_timer = 0;
    if (loop_cnt > 1000)
    {
        Serial1.print("Loop time: (us) ");
        Serial1.println(millis() - loop_timer);
        loop_cnt = 0;
        loop_timer = millis();
    }
    loop_cnt++;
    #endif

    if (player_id == 0)
    {
        master_task();
    }
    else
    {
        slave_task();
    }

    // Handle OG Xbox side
    if (usbd_xid.getType() != usbd_c[0].type)
    {
        usbd_xid.setType(usbd_c[0].type);
    }
    // Timer to limit polling frequency
    static uint32_t poll_timer = 0;
    if (millis() - poll_timer > 4) // Check if 4ms have passed
    {
        #ifdef BLUERETRO
        if (usb_active) // Execute only when flag is true - after slave ping
        {
            // Handle USB device types
            if (usbd_xid.getType() == DUKE)
            {
                UDCON &= ~(1 << DETACH); // Attach USB
                TXLED1; // Turn TX LED on instead of RX - When BLUERETRO is defined, turn on the LED to indicate USB is attached
                usbd_xid.sendReport(&usbd_c[0].duke.in, sizeof(usbd_duke_in_t));
                usbd_xid.getReport(&usbd_c[0].duke.out, sizeof(usbd_duke_out_t));
            }
            else if (usbd_xid.getType() == STEELBATTALION)
            {
                UDCON &= ~(1 << DETACH); // Attach USB
                TXLED1;
                usbd_xid.sendReport(&usbd_c[0].sb.in, sizeof(usbd_sbattalion_in_t));
                usbd_xid.getReport(&usbd_c[0].sb.out, sizeof(usbd_sbattalion_out_t));
            }
            else if (usbd_xid.getType() == DISCONNECTED)
            {
                UDCON |= (1 << DETACH); // Detach USB
                TXLED0; // Turn TX LED off (for BlueRetro) - When BLUERETRO is defined, turn off the LED to indicate USB is detached 
                usb_active = false; // Set flag to false when disconnected
            }
        }
        #else
        // If BLUERETRO is not defined, use RXLED instead of TXLED - legacy behavior
        if (usbd_xid.getType() == DUKE)
        {
            UDCON &= ~(1 << DETACH); // Attach USB
            RXLED1; // Turn RX LED on (for non-BlueRetro)
            usbd_xid.sendReport(&usbd_c[0].duke.in, sizeof(usbd_duke_in_t));
            usbd_xid.getReport(&usbd_c[0].duke.out, sizeof(usbd_duke_out_t));
        }
        else if (usbd_xid.getType() == STEELBATTALION)
        {
            UDCON &= ~(1 << DETACH); // Attach USB
            RXLED1; // Turn RX LED on (for non-BlueRetro)
            usbd_xid.sendReport(&usbd_c[0].sb.in, sizeof(usbd_sbattalion_in_t));
            usbd_xid.getReport(&usbd_c[0].sb.out, sizeof(usbd_sbattalion_out_t));
        }
        else if (usbd_xid.getType() == DISCONNECTED)
        {
            UDCON |= (1 << DETACH); // Detach USB
            RXLED0; // Turn RX LED off (for non-BlueRetro)
        }
        #endif
        poll_timer = millis(); // Update the timer
    }
}
