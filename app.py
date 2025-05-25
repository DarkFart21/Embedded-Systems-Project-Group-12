from flask import Flask, request, jsonify, send_from_directory
import json
import os

app = Flask(__name__, static_folder='static')

DATA_FILE = "data.json"

@app.route('/')
def index():
    return send_from_directory('static', 'index.html')

@app.route('/data', methods=['GET'])
def get_data():
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, 'r') as f:
            return jsonify(json.load(f))
    return jsonify({}), 404

@app.route('/update', methods=['POST'])
def update_data():
    try:
        data = request.get_json()
        with open(DATA_FILE, 'w') as f:
            json.dump(data, f)
        return jsonify({"status": "success"}), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(debug=True)
