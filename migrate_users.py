import firebase_admin
from firebase_admin import credentials, auth, firestore

# Initialize Firebase
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://mango-lens-1fff8-default-rtdb.firebaseio.com'
})

# Initialize Firestore
firestore_db = firestore.client()

def migrate_existing_users():
    print("=== Migrate Existing Users to Firestore ===\n")
    
    try:
        # List all users from Firebase Auth
        users = auth.list_users()
        
        print(f"Found {len(users.users)} user(s) in Firebase Auth:\n")
        
        for idx, user in enumerate(users.users, 1):
            print(f"{idx}. Email: {user.email}")
            print(f"   UID: {user.uid}")
            
            # Check if user already exists in Firestore
            user_ref = firestore_db.collection('users').document(user.uid)
            user_doc = user_ref.get()
            
            if user_doc.exists:
                print(f"   Status: ✅ Already in Firestore")
                print(f"   Role: {user_doc.to_dict().get('role')}\n")
            else:
                # Ask user what role to assign
                print(f"   Status: ❌ Not in Firestore")
                role = input(f"   Enter role for {user.email} (owner/facilitator): ").strip().lower()
                
                if role not in ['owner', 'facilitator']:
                    print(f"   ⚠️  Invalid role. Skipping...\n")
                    continue
                
                username = input(f"   Enter username for {user.email}: ").strip()
                
                # Create user document in Firestore
                user_data = {
                    "email": user.email,
                    "username": username,
                    "role": role,
                    "created_at": firestore.SERVER_TIMESTAMP
                }
                
                user_ref.set(user_data)
                print(f"   ✅ Added to Firestore as {role}\n")
        
        print("Migration completed!")
        
    except Exception as e:
        print(f"\n❌ Error during migration: {str(e)}")

if __name__ == "__main__":
    migrate_existing_users()
