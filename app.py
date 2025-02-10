from flask import Flask, render_template, jsonify, Response
from flask_mqtt import Mqtt
from flask_socketio import SocketIO
import sqlite3
import json
import csv
from datetime import datetime
from io import StringIO

app = Flask(__name__)

# MQTT Settings
app.config['MQTT_BROKER_URL'] = 'broker.emqx.io'
app.config['MQTT_BROKER_PORT'] = 1883
app.config['MQTT_USERNAME'] = ''  # public broker doesn't need credentials
app.config['MQTT_PASSWORD'] = ''
app.config['MQTT_CLIENT_ID'] = 'AmmoniacServer080704'  # Change 123 to random numbers

# Initialize MQTT and SocketIO
mqtt = Mqtt(app)
socketio = SocketIO(app)

def init_db():
    """CREATES THE DATABASE AND TABLES IF THEY DON'T EXIST"""
    conn = sqlite3.connect('sensors.db')
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS readings
                 (timestamp TEXT, sensor_id INTEGER, ammonia REAL)''')
    conn.commit()
    conn.close()

@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    """SUBSCRIBES TO ALL SENSOR TOPICS WHEN CONNECTED"""
    mqtt.subscribe('amoniac/sensor#')

@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):
    """HANDLES INCOMING MQTT MESSAGES"""
    data = json.loads(message.payload.decode())
    
    # Save to database
    conn = sqlite3.connect('sensors.db')
    c = conn.cursor()
    c.execute('INSERT INTO readings VALUES (?, ?, ?)',
              (datetime.now().isoformat(), data['sensor_id'], data['ammonia']))
    conn.commit()
    conn.close()
    
    # Emit to websocket for real-time updates
    socketio.emit('new_reading', data)

@app.route('/')
def index():
    """SERVES THE MAIN PAGE"""
    return render_template('index.html')

@app.route('/current_data')
def get_current_data():
    """RETURNS THE LATEST READINGS FOR EACH SENSOR"""
    conn = sqlite3.connect('sensors.db')
    c = conn.cursor()
    c.execute('''
        SELECT sensor_id, ammonia, timestamp
        FROM readings
        WHERE (sensor_id, timestamp) IN 
            (SELECT sensor_id, MAX(timestamp) 
             FROM readings 
             GROUP BY sensor_id)
    ''')
    readings = c.fetchall()
    conn.close()
    
    return jsonify([{
        'sensor_id': r[0],
        'ammonia': r[1],
        'timestamp': r[2]
    } for r in readings])

@app.route('/download_csv')
def download_csv():
    """GENERATES A CSV FILE OF ALL READINGS"""
    conn = sqlite3.connect('sensors.db')
    c = conn.cursor()
    c.execute('SELECT * FROM readings ORDER BY timestamp DESC')
    
    # Create CSV in memory
    si = StringIO()
    cw = csv.writer(si)
    cw.writerow(['Timestamp', 'Sensor ID', 'Ammonia Level (PPM)'])
    cw.writerows(c.fetchall())
    
    output = si.getvalue()
    conn.close()
    
    return Response(
        output,
        mimetype="text/csv",
        headers={"Content-disposition":
                f"attachment; filename=ammonia_readings_{datetime.now().strftime('%Y%m%d_%H%M')}.csv"}
    )

if __name__ == '__main__':
    init_db()
    socketio.run(app, debug=True)
