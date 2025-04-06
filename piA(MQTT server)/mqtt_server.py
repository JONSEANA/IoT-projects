import os
import threading
import time
import paho.mqtt.client as mqtt
from flask import Flask, jsonify, render_template_string, send_from_directory
from flask_socketio import SocketIO
from flask import make_response

# Flask and SocketIO setup
app = Flask(__name__)
socketio = SocketIO(app)

# MQTT setup (unchanged)
BROKER = "test.mosquitto.org"
PORT = 1883
TOPIC = "pi/mqtt"
TOPIC_LOCK = "lock/mqtt"
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
MQTT_USERNAME = 'jiamin'
MQTT_PASSWORD = 'jiamin'

# File paths (unchanged)
NFS_FOLDER = "/CSC2106/yonghao_images/"

# Store image filenames and thread lock
image_files = []
image_files_lock = threading.Lock()

def scan_nfs_folder():
    global image_files
    with image_files_lock:
        previous_files = image_files.copy()
        new_files = sorted(
            [f for f in os.listdir(NFS_FOLDER) if f.lower().endswith((".jpg", ".jpeg", ".png"))],
            key=lambda x: os.path.getmtime(os.path.join(NFS_FOLDER, x)),
            reverse=True
        )
        if new_files != previous_files:
            image_files = new_files
            print(f"Scanned NFS folder. New images: {image_files}")
            return True
    return False

def on_message(client, userdata, msg):
    received_filename = msg.payload.decode("utf-8").strip()
    print(f"Received MQTT message on topic {msg.topic}: {received_filename}")

    if msg.topic == TOPIC:
        src_file_path = os.path.join(NFS_FOLDER, received_filename)
        if os.path.exists(src_file_path) and received_filename.lower().endswith((".jpg", ".jpeg", ".png")):
            scan_nfs_folder()
            socketio.emit('refresh')
        else:
            print(f"File not found or invalid format: {src_file_path}")    
    elif msg.topic == TOPIC_LOCK:
        print(f"Handle message for topic2: {received_filename}")

@app.route('/images', methods=['GET'])
def get_images():
    with image_files_lock:
        if not image_files:
            return jsonify({"status": "error", "message": "No images available"})
        
        images_info = []
        for filename in image_files:
            filepath = os.path.join(NFS_FOLDER, filename)
            try:
                mtime = os.path.getmtime(filepath)
                size = os.path.getsize(filepath)
                images_info.append({
                    "name": filename,
                    "path": f"/CSC2106/yonghao_images/{filename}",
                    "size": f"{size/1024:.1f} KB",
                    "date": time.strftime('%Y-%m-%d %H:%M', time.localtime(mtime))
                })
            except Exception as e:
                print(f"Error getting info for {filename}: {e}")
                images_info.append({
                    "name": filename,
                    "path": f"/CSC2106/yonghao_images/{filename}",
                    "size": "Unknown",
                    "date": "Unknown"
                })
        
        return jsonify({
            "status": "success", 
            "images_info": images_info,
            "count": len(images_info),
            "last_updated": time.time()
        })

# Unchanged image serving route
@app.route('/CSC2106/yonghao_images/<filename>')
def serve_image(filename):
    response = make_response(send_from_directory(NFS_FOLDER, filename))
    response.headers['Cache-Control'] = 'no-cache, no-store, must-revalidate'
    response.headers['Pragma'] = 'no-cache'
    response.headers['Expires'] = '0'
    return response

# Bootstrap 5 Gallery Template
html_template = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Image Gallery</title>
    <!-- Bootstrap 5 CSS -->
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <!-- Bootstrap Icons -->
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.10.0/font/bootstrap-icons.css">
    <style>
        .gallery-img {
            height: 200px;
            object-fit: cover;
            cursor: pointer;
            transition: transform 0.3s;
        }
        .gallery-img:hover {
            transform: scale(1.02);
        }
        .card {
            transition: all 0.3s;
        }
        .card:hover {
            box-shadow: 0 0.5rem 1rem rgba(0, 0, 0, 0.15);
        }
        .toast {
            position: fixed;
            top: 20px;
            right: 20px;
            z-index: 1100;
        }
        .spinner-border {
            width: 1.5rem;
            height: 1.5rem;
            border-width: 0.2em;
            display: none;
        }
    </style>
</head>
<body class="bg-light">
    <div class="container py-4">
        <div class="row mb-4">
            <div class="col-md-6">
                <h1 class="display-5 fw-bold text-primary">
                    <i class="bi bi-images"></i> Image Gallery
                </h1>
                <p class="text-muted">Automatically updated when new images arrive</p>
            </div>
            <div class="col-md-6 text-md-end">
                <button id="refresh-btn" class="btn btn-primary">
                    <span id="refresh-spinner" class="spinner-border spinner-border-sm"></span>
                    <span id="refresh-text">Refresh</span>
                </button>
                <div class="mt-2">
                    <span class="badge bg-info">
                        <i class="bi bi-image"></i> <span id="image-count">0</span> images
                    </span>
                    <span class="badge bg-secondary ms-2">
                        <i class="bi bi-clock"></i> Updated: <span id="last-updated">Never</span>
                    </span>
                </div>
            </div>
        </div>

        <div id="gallery-container" class="row row-cols-1 row-cols-sm-2 row-cols-md-3 row-cols-lg-4 g-4">
            <!-- Images will be loaded here -->
        </div>

        <div id="empty-state" class="text-center py-5" style="display: none;">
            <i class="bi bi-folder-x text-muted" style="font-size: 3rem;"></i>
            <h3 class="mt-3 text-muted">No images found</h3>
            <p class="text-muted">Images will appear here automatically when available</p>
        </div>
    </div>

    <!-- Image Modal -->
    <div class="modal fade" id="imageModal" tabindex="-1" aria-hidden="true">
        <div class="modal-dialog modal-dialog-centered modal-xl">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title" id="modal-title">Image</h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                </div>
                <div class="modal-body text-center">
                    <img id="modal-image" class="img-fluid" style="max-height: 70vh;">
                </div>
                <div class="modal-footer">
                    <a id="download-btn" class="btn btn-primary" download>
                        <i class="bi bi-download"></i> Download
                    </a>
                    <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">
                        <i class="bi bi-x-lg"></i> Close
                    </button>
                </div>
            </div>
        </div>
    </div>

    <!-- Toast Notification -->
    <div class="toast align-items-center text-white bg-success" role="alert" aria-live="assertive" aria-atomic="true">
        <div class="d-flex">
            <div class="toast-body" id="toast-message"></div>
            <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
    </div>

    <!-- jQuery, Bootstrap JS, and SocketIO -->
    <script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.5.4/socket.io.js"></script>

    <script>
        $(document).ready(function() {
            const socket = io.connect('http://' + document.domain + ':' + location.port);
            const toast = new bootstrap.Toast(document.querySelector('.toast'));
            const imageModal = new bootstrap.Modal(document.getElementById('imageModal'));

            // Load images on page load
            updateGallery();

            // Manual refresh button
            $('#refresh-btn').click(function() {
                $('#refresh-spinner').show();
                $('#refresh-text').text('Refreshing...');
                updateGallery();
            });

            // SocketIO refresh event
            socket.on('refresh', function() {
                showToast('New images loaded!');
                updateGallery();
            });

            // Update gallery function
            function updateGallery() {
                $.get('/images', function(data) {
                    if (data.status === "success" && data.images_info.length > 0) {
                        renderGallery(data.images_info);
                        $('#empty-state').hide();
                    } else {
                        $('#gallery-container').empty();
                        $('#empty-state').show();
                    }
                    $('#image-count').text(data.count);
                    $('#last-updated').text(new Date(data.last_updated * 1000).toLocaleString());
                })
                .always(function() {
                    $('#refresh-spinner').hide();
                    $('#refresh-text').text('Refresh');
                });
            }

            // Render gallery items
            function renderGallery(images) {
                const gallery = $('#gallery-container');
                gallery.empty();
                
                images.forEach(img => {
                    const timestamp = new Date().getTime();
                    gallery.append(`
                        <div class="col">
                            <div class="card h-100">
                                <img src="${img.path}?t=${timestamp}" class="card-img-top gallery-img" 
                                     alt="${img.name}" onclick="openModal('${img.path}?t=${timestamp}', '${img.name}')">
                                <div class="card-body">
                                    <h6 class="card-title text-truncate" title="${img.name}">${img.name}</h6>
                                    <p class="card-text small text-muted">
                                        <i class="bi bi-calendar"></i> ${img.date}<br>
                                        <i class="bi bi-file-earmark"></i> ${img.size}
                                    </p>
                                </div>
                            </div>
                        </div>
                    `);
                });
            }

            // Open modal with clicked image
            window.openModal = function(src, filename) {
                $('#modal-image').attr('src', src);
                $('#modal-title').text(filename);
                $('#download-btn').attr('href', src).attr('download', filename);
                imageModal.show();
            }

            // Show toast notification
            function showToast(message) {
                $('#toast-message').text(message);
                toast.show();
            }
        });
    </script>
</body>
</html>
"""

@app.route('/gallery', methods=['GET'])
def display_images():
    return render_template_string(html_template)

def run_flask():
    socketio.run(app, host="0.0.0.0", port=5000, debug=False, use_reloader=False)

def periodic_scanner():
    while True:
        time.sleep(10)
        print("Performing periodic scan...")
        changed = scan_nfs_folder()
        if changed:
            print("Periodic scan detected changes. Emitting refresh event.")
            socketio.emit('refresh')

# Initial scan
scan_nfs_folder()

# Start threads
flask_thread = threading.Thread(target=run_flask, daemon=True)
flask_thread.start()

scanner_thread = threading.Thread(target=periodic_scanner, daemon=True)
scanner_thread.start()

# MQTT setup
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD) 
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.subscribe([(TOPIC, 0), (TOPIC_LOCK, 0)])

print(f"Listening on '{TOPIC}' and serving gallery at http://{BROKER}:5000/gallery")
client.loop_forever()
