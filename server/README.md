# Click Leaderboard & Global Boulder API (Cloudflare Worker + D1)

Zero-cost ($0/month), zero-maintenance serverless backend for the Click Community Leaderboard.

## 🚀 Quick Deploy in 3 Minutes

### 1. Log in to Cloudflare
In your terminal, navigate to this `server/` directory:
```bash
cd server
npx wrangler login
```

### 2. Create the D1 Database
Run:
```bash
npx wrangler d1 create click_db
```
This command will output your `database_id` (a UUID).
Copy that `database_id` into [`wrangler.toml`](file:///Users/prajjwal/Documents/GitHub/Click/server/wrangler.toml) replacing `YOUR_D1_DATABASE_ID_HERE`.

### 3. Initialize the Tables & Deploy
Run:
```bash
# Apply SQLite Schema to remote D1 database
npx wrangler d1 execute click_db --remote --file=schema.sql

# Deploy the Worker
npx wrangler deploy
```

Once deployed, Wrangler will give you a public URL like:
`https://click-leaderboard-api.<your-subdomain>.workers.dev`

You can also bind a custom domain in Cloudflare dashboard (e.g. `https://api.uprajjwal.com.np`).

---

## 📡 API Endpoints

### `GET /api/leaderboard`
Returns current community boulder stats and Top 10 leaderboards across all three minigames:
```json
{
  "global_boulder_clicks": 1420580,
  "total_devices": 38,
  "top_sisyphus": [
    { "device_name": "Prajjwal's Click", "total_clicks": 35200, "last_synced_at": "2026-09-13T08:00:00Z" }
  ],
  "top_flappy": [
    { "device_name": "SkyMaster", "flappy_high_score": 42, "last_synced_at": "2026-09-13T08:00:00Z" }
  ],
  "top_timing": [
    { "device_name": "PrecisionKing", "just_ten_best_ms": 4, "just_ten_time": 10.004, "last_synced_at": "2026-09-13T08:00:00Z" }
  ]
}
```

### `POST /api/sync`
Syncs stats from Web Flasher with human-speed rate limiting:
```json
{
  "chip_id": "3C71BF89A1B2",
  "name": "Prajjwal's Click",
  "clicks": 35200,
  "flappy": 42,
  "just_ten": 10.004,
  "uptime_hrs": 12
}
```
Response:
```json
{
  "success": true,
  "credited_delta": 350,
  "total_boulder_clicks": 1420580,
  "message": "Successfully synced! +350 clicks added to the Community Boulder."
}
```

### `GET /api/user?device_id=3C71BF89A1B2`
Retrieves registered player profile and stats for the given hardware chip ID:
```json
{
  "found": true,
  "device": {
    "chip_id": "3C71BF89A1B2",
    "device_name": "Prajjwal's Click",
    "total_clicks": 35200,
    "flappy_high_score": 42,
    "just_ten_time": 10.004,
    "just_ten_best_ms": 4
  }
}
```

---

## 🛡️ Anti-Cheat & Community Boulder Engine

1. **Human Click Velocity Validation**:
   - Caps delta clicks at humanly possible physical boundaries ($\le 20\text{ clicks/second}$).
   - Any client claiming abnormal click deltas relative to elapsed time since last sync is rejected or clamped to prevent botting/scripting.
2. **Atomic D1 SQLite Transactions**:
   - Updates both the individual device profile in `devices` and the collective boulder count in `global_stats` within an ACID transaction.
3. **Audit Trail**:
   - All claims are stored in `sync_log` with timestamp, claimed delta, credited delta, and hashed client IP.

