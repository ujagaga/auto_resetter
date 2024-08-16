## auto_resetter

My router periodically needs a power down for at least 10 seconds to work well. 
I am using ESP8266 as an NTP clock, which triggers a Relay and powers off my router.

# How to start

1. Open the auto_resetter.ino in Arduino IDE and locate at the start of the file:

        #define STASSID       "WiFi_SSID"
        #define STAPSK        "WiFi_Pass"

2. Replace the "WiFi_SSID" and "WiFi_Pass" with your WiFi network name and password.
3. Adjust to your setup the LED and Relay pin number.

        #define LED_PIN       (2)
        #define RELAY_PIN     (4)

4. If you do not have ESP8266 board support already installed, add

        https://arduino.esp8266.com/stable/package_esp8266com_index.json

to your Additional Boards Manager URLs and select appropriate ESP8266 board and port.
5. Build and program the board.
6. Connect the relay to the ESP8266 board and use the relay as a way to turn on/off the power to your router.

# NOTE

If you do not already have the material, I reccoment buying a development board wioth power supply, 
ESP-12 module and a relay already on board.

 
# Contact

ujagaga@gmail.com