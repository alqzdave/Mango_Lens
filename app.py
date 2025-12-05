from flask import Flask, render_template, request, redirect, url_for, session, flash, jsonify
import firebase_admin
from firebase_admin import credentials, auth, db, firestore
from functools import wraps
import os
import json

app = Flask(__name__)
app.secret_key = "2058a9b39e411a8444c125cf0ea243fc"

# Use environment variable for Firebase credentials in production
if os.getenv('FIREBASE_CREDENTIALS'):
    # DigitalOcean: credentials from environment variable
    cred_dict = json.loads(os.getenv('FIREBASE_CREDENTIALS'))
    cred = credentials.Certificate(cred_dict)
else:
    # Local development: use serviceAccountKey.json file
    cred = credentials.Certificate("serviceAccountKey.json")

firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://mango-lens-1fff8-default-rtdb.firebaseio.com'
})

# Initialize Firestore for user data
firestore_db = firestore.client()


def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if "user_uid" not in session:
            flash("You must be logged in to access this page.", "danger")
            return redirect(url_for("login"))
        return f(*args, **kwargs)
    return decorated_function


def owner_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if "user_uid" not in session:
            flash("You must be logged in to access this page.", "danger")
            return redirect(url_for("login"))
        
        # Check if user is owner
        user_ref = firestore_db.collection('users').document(session['user_uid'])
        user_doc = user_ref.get()
        
        if user_doc.exists:
            user_data = user_doc.to_dict()
            if user_data.get('role') != 'owner':
                flash("You don't have permission to access this page.", "danger")
                return redirect(url_for('index'))
        else:
            flash("User data not found.", "danger")
            return redirect(url_for('login'))
        
        return f(*args, **kwargs)
    return decorated_function


@app.route('/')
def index():
    # Check if user is logged in
    user_role = None
    if 'user_uid' in session:
        # Get user role from Firestore
        user_ref = firestore_db.collection('users').document(session['user_uid'])
        user_doc = user_ref.get()
        
        if user_doc.exists:
            user_data = user_doc.to_dict()
            user_role = user_data.get('role', 'facilitator')
    
    return render_template('index.html', user_role=user_role)


@app.route('/account', methods=['GET', 'POST'])
@owner_required  
def account():
    if "idToken" not in session:
        flash("You must be logged in to create accounts.", "danger")
        return redirect(url_for('login'))

    if request.method == 'POST':
        username = request.form['username']
        email = request.form['email']
        password = request.form['password']
        role = request.form['role']

        try:
            # Create user in Firebase Auth
            new_user = auth.create_user(email=email, password=password)

            # Store user data in Firestore (instead of Realtime Database)
            user_data = {
                "email": email,
                "username": username,
                "role": role,
                "created_at": firestore.SERVER_TIMESTAMP
            }

            # Save to Firestore 'users' collection
            firestore_db.collection('users').document(new_user.uid).set(user_data)
            
            flash("Account created successfully!", "success")
            return redirect(url_for('login'))

        except Exception as e:
            flash(f"Error creating account: {str(e)}", "danger")
            return redirect(url_for('account'))

    return render_template('account.html')


@app.route('/login', methods=['GET'])
def login():
    return render_template('login.html')


@app.route('/login', methods=['POST'])
def login_post():
    try:
        id_token = request.json.get('idToken')

        if not id_token:
            return jsonify({'error': 'No ID token provided'}), 400

        decoded_token = auth.verify_id_token(id_token)
        user_uid = decoded_token['uid']
        
        # Check if user exists in Firestore
        user_ref = firestore_db.collection('users').document(user_uid)
        user_doc = user_ref.get()

        if not user_doc.exists:
            # Create new user in Firestore if doesn't exist
            user_info = {
                "email": decoded_token['email'],
                "username": "Facilitator",  
                "role": "facilitator",
                "created_at": firestore.SERVER_TIMESTAMP
            }
            user_ref.set(user_info)
        
        # Get user role
        user_doc = user_ref.get()
        user_data = user_doc.to_dict()
        user_role = user_data.get('role', 'facilitator')

        # Store session data
        session['user_uid'] = user_uid
        session['idToken'] = id_token
        session['user_role'] = user_role

        return jsonify({'success': True}), 200

    except Exception as e:
        return jsonify({'error': f'Login failed: {str(e)}'}), 500


@app.route('/logout')
@login_required  
def logout():
    session.clear()
    flash("Logged out successfully", "success")
    return redirect(url_for('login'))


@app.route('/record')
@login_required
def record():
    # Get user role from Firestore
    user_ref = firestore_db.collection('users').document(session['user_uid'])
    user_doc = user_ref.get()
    user_role = 'facilitator'  # default
    
    if user_doc.exists:
        user_data = user_doc.to_dict()
        user_role = user_data.get('role', 'facilitator')
    
    return render_template('record.html', user_role=user_role)

@app.route('/history')
def history():
    # This is where you would pull the historical data (e.g., from a database)
    return render_template('history.html')


@app.route('/sorting')
@login_required
def sorting():
    # Get user role from Firestore
    user_ref = firestore_db.collection('users').document(session['user_uid'])
    user_doc = user_ref.get()
    user_role = 'facilitator'  # default
    
    if user_doc.exists:
        user_data = user_doc.to_dict()
        user_role = user_data.get('role', 'facilitator')
    
    return render_template('sorting.html', user_role=user_role)


@app.route('/api/sorting-data', methods=['POST'])
def receive_sorting_data():
    """
    API endpoint for IoT device (ESP32-CAM) to send mango sorting data
    Expected JSON format:
    {
        "variety": "Carabao" | "Pico" | "Indian" (optional - will use selected variety),
        "ripeness": "Ripe" | "Unripe" | "Overripe",
        "count": 1,
        "confidence": 95.5 (optional)
    }
    """
    global selected_variety
    
    try:
        data = request.get_json()
        
        # Validate required fields
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        # Use selected variety from dashboard, ignore ESP32 variety
        variety = selected_variety or 'Carabao'
        ripeness = data.get('ripeness')
        count = data.get('count', 1)
        confidence = data.get('confidence', 0)
        
        if not ripeness:
            return jsonify({'error': 'Missing required field: ripeness'}), 400
        
        # Validate ripeness
        valid_ripeness = ['Ripe', 'Unripe', 'Overripe']
        if ripeness not in valid_ripeness:
            return jsonify({'error': f'Invalid ripeness. Must be one of: {valid_ripeness}'}), 400
        
        # Prepare data for Firebase
        from datetime import datetime
        import time
        
        # Format date without zero-padding the day: "December 3, 2025" not "December 03, 2025"
        now = datetime.now()
        current_date = f"{now.strftime('%B')} {now.day}, {now.year}"
        timestamp = int(time.time() * 1000)  # milliseconds
        
        record_data = {
            "date": current_date,
            "variety": variety,  # Use selected variety from dashboard
            "ripeness": ripeness,
            "count": int(count),
            "confidence": float(confidence),
            "timestamp": timestamp
        }
        
        # Save to Firebase Realtime Database
        print(f"\n[FIREBASE SAVE] Date: {current_date}, Variety: {variety}, Ripeness: {ripeness}, Count: {count}, Confidence: {confidence}%")
        try:
            result = db.reference('records').push(record_data)
            print(f"[FIREBASE SUCCESS] ✓ Saved with key: {result.key}")
            print(f"[FIREBASE SUCCESS] ✓ Record: {record_data}\n")
        except Exception as e:
            print(f"[FIREBASE ERROR] ✗ Failed to save: {str(e)}\n")
            return jsonify({'error': 'Failed to save to database', 'details': str(e)}), 500
        
        return jsonify({
            'success': True,
            'message': 'Sorting data saved successfully',
            'data': record_data
        }), 201
        
    except Exception as e:
        return jsonify({'error': f'Failed to save data: {str(e)}'}), 500


# Global variables to store ESP32-CAM IP, selected variety, and sorting state
esp32_cam_ip = None
selected_variety = 'Carabao'  # Default variety

# State file path for persistent sorting state
STATE_FILE = 'sorting_state.json'

def get_sorting_state():
    """Read sorting state from file"""
    try:
        if os.path.exists(STATE_FILE):
            with open(STATE_FILE, 'r') as f:
                data = json.load(f)
                return data.get('active', False)
    except:
        pass
    return False

def set_sorting_state(active):
    """Save sorting state to file"""
    try:
        with open(STATE_FILE, 'w') as f:
            json.dump({'active': active}, f)
    except Exception as e:
        print(f"Error saving state: {e}")

@app.route('/api/set-camera-ip', methods=['POST'])
def set_camera_ip():
    """Set the ESP32-CAM IP address"""
    global esp32_cam_ip
    try:
        data = request.get_json()
        ip = data.get('ip')
        if ip:
            esp32_cam_ip = ip
            return jsonify({'success': True, 'message': f'Camera IP set to {ip}'}), 200
        return jsonify({'error': 'No IP provided'}), 400
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/get-camera-ip', methods=['GET'])
def get_camera_ip():
    """Get the stored ESP32-CAM IP address"""
    global esp32_cam_ip
    return jsonify({'ip': esp32_cam_ip}), 200


@app.route('/api/set-variety', methods=['POST'])
def set_variety():
    """Set the currently selected mango variety"""
    global selected_variety
    try:
        data = request.get_json()
        variety = data.get('variety')
        valid_varieties = ['Carabao', 'Pico']
        
        if variety and variety in valid_varieties:
            selected_variety = variety
            return jsonify({'success': True, 'message': f'Variety set to {variety}'}), 200
        return jsonify({'error': 'Invalid variety'}), 400
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/get-variety', methods=['GET'])
def get_variety():
    """Get the currently selected variety"""
    global selected_variety
    return jsonify({'variety': selected_variety}), 200

@app.route('/api/sorting-control', methods=['POST', 'GET'])
def sorting_control():
    """Control ESP32 sorting - start/stop"""
    if request.method == 'POST':
        data = request.get_json()
        active = data.get('active', False)
        set_sorting_state(active)
        print(f"[SORTING CONTROL] Sorting {'STARTED' if active else 'STOPPED'}")
        print(f"[SORTING CONTROL] State saved to file: {active}")
        return jsonify({'success': True, 'active': active}), 200
    else:
        # ESP32 checks this to know if it should continue sorting
        active = get_sorting_state()
        print(f"[SORTING CONTROL GET] Current state from file: {active}")
        return jsonify({'active': active}), 200


@app.route('/api/get-latest-sorting-data', methods=['GET'])
def get_latest_sorting_data():
    """Get the most recent sorting data from Firebase"""
    try:
        # Query Firebase for all records
        records_ref = db.reference('records')
        records = records_ref.get()
        
        if records:
            # Convert to list and find the latest by timestamp
            latest_record = None
            latest_timestamp = 0
            
            for key, record in records.items():
                if record.get('timestamp', 0) > latest_timestamp:
                    latest_timestamp = record['timestamp']
                    latest_record = record
            
            if latest_record:
                return jsonify({
                    'success': True,
                    'data': latest_record
                }), 200
        
        return jsonify({
            'success': False,
            'message': 'No records found'
        }), 200
            
    except Exception as e:
        return jsonify({'error': f'Failed to fetch data: {str(e)}'}), 500


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')
