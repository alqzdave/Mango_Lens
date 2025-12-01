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


@app.route('/api/sorting-data', methods=['POST'])
def receive_sorting_data():
    """
    API endpoint for IoT device (ESP32-CAM) to send mango sorting data
    Expected JSON format:
    {
        "variety": "Carabao" | "Pico" | "Indian",
        "ripeness": "Ripe" | "Unripe" | "Overripe",
        "count": 1
    }
    """
    try:
        data = request.get_json()
        
        # Validate required fields
        if not data:
            return jsonify({'error': 'No data provided'}), 400
        
        variety = data.get('variety')
        ripeness = data.get('ripeness')
        count = data.get('count', 1)
        
        if not variety or not ripeness:
            return jsonify({'error': 'Missing required fields: variety and ripeness'}), 400
        
        # Validate variety
        valid_varieties = ['Carabao', 'Pico', 'Indian']
        if variety not in valid_varieties:
            return jsonify({'error': f'Invalid variety. Must be one of: {valid_varieties}'}), 400
        
        # Validate ripeness
        valid_ripeness = ['Ripe', 'Unripe', 'Overripe']
        if ripeness not in valid_ripeness:
            return jsonify({'error': f'Invalid ripeness. Must be one of: {valid_ripeness}'}), 400
        
        # Prepare data for Firebase
        from datetime import datetime
        import time
        
        current_date = datetime.now().strftime("%B %d, %Y")
        timestamp = int(time.time() * 1000)  # milliseconds
        
        record_data = {
            "date": current_date,
            "variety": variety,
            "ripeness": ripeness,
            "count": int(count),
            "timestamp": timestamp
        }
        
        # Save to Firebase Realtime Database
        db.reference('records').push(record_data)
        
        return jsonify({
            'success': True,
            'message': 'Sorting data saved successfully',
            'data': record_data
        }), 201
        
    except Exception as e:
        return jsonify({'error': f'Failed to save data: {str(e)}'}), 500


if __name__ == '__main__':
    app.run(debug=True)
