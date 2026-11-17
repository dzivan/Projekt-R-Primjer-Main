import json

import paho.mqtt.client as mqtt
#Immediately just subscribe to the raw readings from the sensor
def on_connect(client, userdata, flags, reason_code, properties):
    client.subscribe("esp32/light")


#On every message, do:
def on_message(client, userdata, msg):
    try:
        #Try extracting the light value from the message
        lux = float(json.loads(msg.payload)["lux"])
    except Exception:
        print("error")
        #If it doesn't work, ignore the message
        return
    if lux > 200.0:
        print("Test trigger")
        #If the value is higher than say 200 lux, post it to the alert channel
        client.publish("esp32/alert", json.dumps({"lux": lux}), qos = 0)

#Set the client up
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
#Localhost connect because the script is running on the edge which is local (same network as ESP32)
client.connect("localhost", 1883)
client.loop_forever()