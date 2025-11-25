// Firebase Configuration
// This file contains the Firebase configuration and initialization
// Include this file after loading Firebase SDK scripts

import firebase from "firebase/app"
import "firebase/database"

const firebaseConfig = {
  apiKey: "AIzaSyCoYkWwdq7wm5nhUjGZ47FtUm1XDyVzOsQ",
  authDomain: "mango-lens.firebaseapp.com",
  databaseURL: "https://mango-lens-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "mango-lens",
  storageBucket: "mango-lens.firebasestorage.app",
  messagingSenderId: "1044561936701",
  appId: "1:1044561936701:web:c3e6f459c0c22a17af0554",
}

// Initialize Firebase
firebase.initializeApp(firebaseConfig)

// Export database reference for use in other files
const db = firebase.database()
