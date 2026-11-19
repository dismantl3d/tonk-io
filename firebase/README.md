# Firebase setup

Waiting on your Firebase web config. When you have it, paste it into `web/firebase-config.js` (gitignored), shaped like:

```js
export const firebaseConfig = { apiKey: "...", authDomain: "...", projectId: "...", /* ... */ };
```

Then:
1. Enable Authentication (Anonymous + Google) and Firestore in the console.
2. Deploy rules: `firebase deploy --only firestore:rules` (uses `firestore.rules` here).
3. Create a service account key for the game server (never commit it; set `GOOGLE_APPLICATION_CREDENTIALS`).
4. Seed `catalog` and `legendaryPath` from GAME_DESIGN.md sections 9–11.

Note: Firebase has no official C++ Admin SDK. The server verifies ID tokens (RS256 JWT against Google's public certs) and writes results via the Firestore REST API with the service account.
