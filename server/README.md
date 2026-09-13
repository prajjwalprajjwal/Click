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
Returns current community boulder stats and Top 10 leaderboards:
```json
{
  "global_boulder_clicks": 1420580,
  "total_devices": 38,
  "top_sisyphus": [
    { "device_name": "Prajjwal's Click", "total_clicks": 35200, "last_synced_at": "2026-09-12T12:00:00Z" }
  ],
  "top_flappy": [
    { "device_name": "SkyMaster", "flappy_high_score": 42, "last_synced_at": "2026-09-12T12:00:00Z" }
  ]
}
```

### `POST /api/sync`
Syncs stats from Web Flasher with human-speed rate limiting:
```json
{
  "chip_id": "3C71BF89A1B2",
  "name": "My Click",
  "clicks": 24150,
  "flappy": 18,
  "uptime_hrs": 36
}
```
Response:
```json
{
  "success": true,
  "credited_delta": 4150,
  "total_boulder_clicks": 1424730,
  "message": "Successfully synced! +4,150 clicks added to the Community Boulder."
}
```
