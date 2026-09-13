-- D1 SQLite Schema for Click Community Leaderboard

CREATE TABLE IF NOT EXISTS global_stats (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    total_boulder_clicks INTEGER NOT NULL DEFAULT 0,
    total_devices INTEGER NOT NULL DEFAULT 0,
    last_updated DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Initialize global row if not exists
INSERT OR IGNORE INTO global_stats (id, total_boulder_clicks, total_devices) VALUES (1, 0, 0);

CREATE TABLE IF NOT EXISTS devices (
    chip_id TEXT PRIMARY KEY,
    device_name TEXT NOT NULL,
    total_clicks INTEGER NOT NULL DEFAULT 0,
    last_synced_clicks INTEGER NOT NULL DEFAULT 0,
    last_synced_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    flappy_high_score INTEGER NOT NULL DEFAULT 0,
    just_ten_time REAL NOT NULL DEFAULT 0,
    just_ten_best_ms INTEGER NOT NULL DEFAULT 0,
    uptime_hrs INTEGER NOT NULL DEFAULT 0,
    verified INTEGER NOT NULL DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS sync_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chip_id TEXT NOT NULL,
    delta_claimed INTEGER NOT NULL,
    delta_credited INTEGER NOT NULL,
    ip_hash TEXT,
    synced_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_devices_clicks ON devices(total_clicks DESC);
CREATE INDEX IF NOT EXISTS idx_devices_flappy ON devices(flappy_high_score DESC);
