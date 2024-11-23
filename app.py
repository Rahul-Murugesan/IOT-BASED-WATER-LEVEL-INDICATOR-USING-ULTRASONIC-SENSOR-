import paho.mqtt.client as mqtt
import time
from twilio.rest import Client

# MQTT callback functions
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker successfully")
        # Subscribe to the desired topic
        client.subscribe("esp32/relay_status")
    else:
        print(f"Failed to connect, return code {rc}")
        
def send_sms_twilio(account_sid, auth_token, from_number, to_number, message):
    client = Client(account_sid, auth_token)

    try:
        message = client.messages.create(
            body=message,
            from_=from_number,
            to=to_number
        )
        print(f"Message sent successfully: {message.sid}")
    except Exception as e:
        print(f"Failed to send message: {e}")

# Example usage
account_sid = ''  # Replace with your Account SID
auth_token = ''    # Replace with your Auth Token
from_number = ''       # Replace with your Twilio phone number
to_number = ''         # Replace with the recipient's phone number
message = 'Water Tank is full...!!! Please Turn of the Motor'

def on_message(client, userdata, msg):
    try:
        # Decode the received MQTT message
        data = msg.payload.decode()
        print(f"Subscribed Data: {data}")

        if data == "1":
            send_sms_twilio(account_sid, auth_token, from_number, to_number,message)
            print("Data is 1, leaving loop for 5 minutes...")
            # Stop listening and pause for 5 minutes
            client.loop_stop()
            time.sleep(300)  # Pause for 300 seconds (5 minutes)
            print("Re-entering the loop...")
            client.loop_start()  # Restart listening
    except Exception as e:
        print(f"Error processing message: {e}")

# Main MQTT setup
broker = "broker.hivemq.com"  # Replace with your MQTT broker address
port = 1883               # Replace with your broker's port if needed

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    # Connect to the MQTT broker
    client.connect(broker, port, 60)
    print("Connecting to broker...")
    # Start the MQTT loop to listen for messages
    client.loop_start()  # Start the loop in the background
    while True:
        time.sleep(1)  # Keep the script alive
except Exception as e:
    print(f"Failed to connect to MQTT broker: {e}")
finally:
    client.loop_stop()
    client.disconnect()
