# sonoff-ha #

Control a Sonoff Basic switch from HomeAssistant.

This implementation is kept simple. It uses a TLS based MQTT client to
communicate with HA. The status of the switch is published on topic
`sonoff/relay` and it subscribes to topic `sonoff/relay/set` for
`on`/`off` commands. The client id has the form `sonoff-XXXXXX` with
the ESP8266's chip id as the last six characters.

HA configuration:

```
light:
  - platform: mqtt
    name: "sonoff-XXXXXX"
    command_topic: "sonoff/relay/set"
    state_topic: "sonoff/relay"
    payload_on: "on"
    payload_off: "off"
```

Pressing the button on the Sonoff switch for more than 5 seconds will
reboot the device.

Don't forget to use signed OTA updates as described
[here](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#advanced-security-signed-updates).

See:
 * [Arduino core for ESP8266 WiFi chip](https://github.com/esp8266/Arduino#arduino-core-for-esp8266-wifi-chip)
 * [ESP8266 Arduino Core’s documentation](https://arduino-esp8266.readthedocs.io/en/latest/index.html)
 ^ [Home Assistant](https://www.home-assistant.io/)
 * [MQTT Light](https://www.home-assistant.io/integrations/light.mqtt/) 
 * [Using Home Assistant to Expand Your Home Automations](https://learn.sparkfun.com/tutorials/using-home-assistant-to-expand-your-home-automations/introduction)
