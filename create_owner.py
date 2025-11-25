import firebase_admin
from firebase_admin import credentials, auth, firestore

# Initialize Firebase
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://mango-lens-1fff8-default-rtdb.firebaseio.com'
})

# Initialize Firestore
firestore_db = firestore.client()

def create_owner_account():
    print("=== Create Owner Account ===\n")
    
    email = input("Enter owner email: ")
    password = input("Enter owner password (min 6 characters): ")
    username = input("Enter owner username: ")
    
    try:
        # Create user in Firebase Auth
        user = auth.create_user(
            email=email,
            password=password
        )
        
        # Store owner data in Firestore
        owner_data = {
            "email": email,
            "username": username,
            "role": "owner",
            "created_at": firestore.SERVER_TIMESTAMP
        }
        
        firestore_db.collection('users').document(user.uid).set(owner_data)
        
        print(f"\n✅ Owner account created successfully!")
        print(f"   Email: {email}")
        print(f"   Username: {username}")
        print(f"   Role: owner")
        print(f"   UID: {user.uid}")
        
    except Exception as e:
        print(f"\n❌ Error creating owner account: {str(e)}")

if __name__ == "__main__":
    create_owner_account()
