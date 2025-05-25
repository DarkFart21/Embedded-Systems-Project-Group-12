from flask import Flask, request, jsonify, send_from_directory
import os
import json
import requests

app = Flask(__name__, static_folder='static')
DATA_FILE = "data.json"
BLYNK_TOKEN = os.environ.get("BLYNK_AUTH_TOKEN")

# Define the pins you care about
VIRTUAL_PINS = {
    "lt": "V2",
    "rt": "V3",
    "ld": "V4",
    "rd": "V5",
    "dvert": "V6",
    "dhoriz": "V7",
    "servovert": "V8",
    "servohori": "V9"
}

@app.route('/')
def index():
    return send_from_directory('static', 'index.html')

@app.route('/data', methods=['GET'])
def get_data():
    # Try to read from data.json (last fetched)
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, 'r') as f:
            return jsonify(json.load(f))
    return jsonify({}), 404

@app.route('/update', methods=['POST'])
def update_data():
    if not BLYNK_TOKEN:
        return jsonify({"error": "Missing Blynk token"}), 500

    data = {}
    for key, pin in VIRTUAL_PINS.items():
        res = requests.get(f"https://sgp1.blynk.cloud/external/api/get?token={BLYNK_TOKEN}&{pin}")
        if res.status_code == 200:
            try:
                data[key] = int(res.text)
            except:
                data[key] = res.text
        else:
            data[key] = None  # mark unavailable pin

    # Save to file
    with open(DATA_FILE, 'w') as f:
        json.dump(data, f)

    return jsonify({"status": "updated", "data": data}), 200

if __name__ == '__main__':
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port)
