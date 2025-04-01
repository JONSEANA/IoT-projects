import base64
import subprocess
import paho.mqtt.client as mqtt
from io import BytesIO
import time
import datetime
import uuid

# MQTT Settings
MQTT_BROKER = "192.168.59.37"
MQTT_PORT = 8883
MQTT_TOPIC = "pi/mqtt"
STATUS_TOPIC = "parcel/status"

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    client.subscribe(STATUS_TOPIC)

def on_message(client, userdata, msg):
    print(f"Message received: {msg.topic} {msg.payload.decode()}")
    if msg.topic == STATUS_TOPIC:
        image_data = capture_image()
        if image_data:
            info = client.publish(MQTT_TOPIC, image_data)
            print(info)
            print("Image sent via MQTT")
            
def capture_image():
    # Generate a unique filename with a timestamp or UUID
    current_time = datetime.datetime.now().strftime("%Y%m%d%H%M%S")
    unique_id = str(uuid.uuid4())
    filename = f"image_{current_time}_{unique_id}.jpg"
    image_path = f"/CSC2106/images/{filename}"

    # Capture an image from the PiCamera
    command = f"libcamera-jpeg -o {image_path}"
    result = subprocess.run(command, shell=True, capture_output=True)
    if result.returncode != 0:
        print("Failed to capture image:", result.stderr.decode())
        return None
    print(f"Image captured successfully: {image_path}")

    # Read the image and encode it in base64
    with open(image_path, "rb") as image_file:
        encoded_string = base64.b64encode(image_file.read()).decode()

    return encoded_string

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message  # Set the on_message callback

    client.username_pw_set('jiamin', 'jiamin')
    client.connect(MQTT_BROKER, MQTT_PORT, 60)

    try:
        client.loop_forever()
    finally:
        client.disconnect()

if __name__ == "__main__":
    main()
