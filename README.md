# sonoff-ha #

Control a Sonoff Basic (R2) switch from HomeAssistant.

This implementation is kept very simple. It uses a client to
communicate with HA and uses MQTT discovery to make new instances
known to HA. The ESP8266's chip id is used as `unique_id` and as the
wifi station name and the light's name, the client id has the form
`sonoff-R2-XXXXXX` with the ESP8266's chip id as the last six
characters.

In a first attempt, TLS was used for the MQTT client, but it took ages
for the client to connect to the broker. For the moment, a non-secure
client is used.

Pressing the button on the Sonoff switch for more than 5 seconds will
reboot the device.

OTA is used for reprogramming the device. Don't forget to use signed
OTA updates as described
[here](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#advanced-security-signed-updates).

MQTT Topics used:

 * homeassistant/light/sonoff-R2-XXXXXX/config for discovery,
 * homeassistant/light/sonoff-R2-XXXXXX/state for the switche's state
   (`"ON"` or `"OFF"`)
 * homeassistant/light/sonoff-R2-XXXXXX/command to switch `"ON"` or
   `"OFF"`

The configuration payload has the form:
```
{"~": "homeassistant/light/sonoff-R2-XXXXXX",
 "name": "sonoff-R2-XXXXXX",
 "unique_id": "XXXXXX",
 "cmd_t": "~/command",
 "stat_t": "~/stat"
}
```
which leaves most values to their defualts.

See:
 * [ha_mqtt_light.ino](https://github.com/smrtnt/Open-Home-Automation) nearly perfect sketch.
 * [Arduino core for ESP8266 WiFi chip](https://github.com/esp8266/Arduino#arduino-core-for-esp8266-wifi-chip)
 * [ESP8266 Arduino Core’s documentation](https://arduino-esp8266.readthedocs.io/en/latest/index.html)
 * [Home Assistant](https://www.home-assistant.io/)
 * [MQTT Light](https://www.home-assistant.io/integrations/light.mqtt/) 
 * [Using Home Assistant to Expand Your Home Automations](https://learn.sparkfun.com/tutorials/using-home-assistant-to-expand-your-home-automations/introduction)
