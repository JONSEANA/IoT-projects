import os
import socket
import subprocess
import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.image import MIMEImage

smtp_server = 'smtp.mail.yahoo.com'
smtp_port = 587
sender_email = 'Your_yahoo_email'
sender_password = 'One_time_password'
recipient_email = 'Receipient_email'


def capture_and_send_image(conn):

    # Open the image file and read its content
    with open('test.jpg', 'rb') as file:
        image_data = file.read()

    # Send the image data over the socket connection
    conn.sendall(image_data)

    print("Image sent successfully")
    return
    

def capture_and_send_email():
    command = "libcamera-still -o test.jpg"
    subprocess.run(command, shell=True)
    print("Image captured successfully")

    msg = MIMEMultipart()
    msg['From'] = sender_email
    msg['To'] = recipient_email
    msg['Subject'] = 'Parcel Detection'
    body = 'Movement Detected'
    msg.attach(MIMEText(body, 'plain'))

    with open('test.jpg', 'rb') as fp:
        img = MIMEImage(fp.read())
        msg.attach(img)

    with smtplib.SMTP(smtp_server, smtp_port) as server:
        server.starttls()
        server.login(sender_email, sender_password)
        server.send_message(msg)
    print("Email sent successfully")

    # Delete the image file after sending the email
    return

def main():
    host = '0.0.0.0' #Change this to YongHao Pi IP
    port = 12346

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print("Waiting for Connection...")
        while True:
            conn, addr = s.accept()
            with conn:
                print("Connected by", addr)
                if addr[0] == '192.168.1.3':
                    capture_and_send_image(conn)
                elif addr[0] == '192.168.1.2':
                    data = conn.recv(1024)
                    if data.decode():
                        capture_and_send_email()
    socket.close()
    
if __name__ == "__main__":
    main()