/**
 * Cloudflare Worker: Click Community Leaderboard & Global Boulder Sync
 * Database: Cloudflare D1 (SQLite)
 */

function corsHeaders(request) {
  const origin = request.headers.get("Origin") || "*";
  return {
    "Access-Control-Allow-Origin": origin,
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    "Access-Control-Max-Age": "86400",
  };
}

function sanitizeText(str, maxLen = 24) {
  if (!str || typeof str !== "string") return "CLICKER";
  const cleaned = str.replace(/[^\w\s\-\.\!\@\#\$\%\^\&\*\(\)\_]/gi, "").trim();
  return (cleaned || "CLICKER").slice(0, maxLen);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const method = request.method.toUpperCase();
    const headers = corsHeaders(request);

    // Handle CORS preflight
    if (method === "OPTIONS") {
      return new Response(null, { status: 204, headers });
    }

    // Health check
    if (url.pathname === "/" || url.pathname === "/api/health") {
      return new Response(JSON.stringify({ status: "ok", service: "click-leaderboard-api" }), {
        headers: { ...headers, "Content-Type": "application/json" },
      });
    }

    // GET /api/leaderboard
    if (method === "GET" && url.pathname === "/api/leaderboard") {
      try {
        const db = env.DB;
        if (!db) {
          return new Response(JSON.stringify({ error: "Database not bound" }), {
            status: 500,
            headers: { ...headers, "Content-Type": "application/json" },
          });
        }

        // Global stats
        const globalRow = await db.prepare(
          "SELECT total_boulder_clicks, total_devices, last_updated FROM global_stats WHERE id = 1"
        ).first();

        // Top 10 Sisyphus clickers
        const topSisyphus = await db.prepare(
          "SELECT chip_id, device_name, total_clicks, last_synced_at FROM devices WHERE total_clicks > 0 ORDER BY total_clicks DESC LIMIT 10"
        ).all();

        // Top 10 Just Ten precision records (lowest deviation)
        const topJustTen = await db.prepare(
          "SELECT chip_id, device_name, just_ten_best_ms, last_synced_at FROM devices WHERE just_ten_best_ms > 0 ORDER BY just_ten_best_ms ASC LIMIT 10"
        ).all();

        // Top 10 Flappy Bird flyers
        const topFlappy = await db.prepare(
          "SELECT chip_id, device_name, flappy_high_score, last_synced_at FROM devices WHERE flappy_high_score > 0 ORDER BY flappy_high_score DESC LIMIT 10"
        ).all();

        function maskSerial(id) {
          if (!id) return "--";
          const str = String(id).replace(/[^A-Za-z0-9]/g, "").toUpperCase();
          if (str.length >= 4) return str.slice(0, 2) + "......" + str.slice(-2);
          return str;
        }

        const totalBoulderClicks = globalRow ? globalRow.total_boulder_clicks : 0;
        const totalDevices = globalRow ? globalRow.total_devices : 0;
        const targetClicks = 1000000;

        const sisyphusList = (topSisyphus && topSisyphus.results || []).map((row, idx) => ({
          rank: idx + 1,
          device_serial: maskSerial(row.chip_id),
          chip_id: maskSerial(row.chip_id),
          name: row.device_name,
          score: row.total_clicks,
          display_score: Number(row.total_clicks).toLocaleString(),
          tier: row.total_clicks >= 20000 ? "Titan" : row.total_clicks >= 10000 ? "Colossus" : "Pusher",
          synced_at: row.last_synced_at
        }));

        const justTenList = (topJustTen && topJustTen.results || []).map((row, idx) => {
          const devSec = (row.just_ten_best_ms / 1000).toFixed(4);
          return {
            rank: idx + 1,
            device_serial: maskSerial(row.chip_id),
            chip_id: maskSerial(row.chip_id),
            name: row.device_name,
            score: row.just_ten_best_ms,
            display_score: `${(10 + row.just_ten_best_ms / 1000).toFixed(4)}s`,
            deviation_display: `+${devSec}s`,
            tier: row.just_ten_best_ms <= 2 ? "Zen Master" : "Focused",
            synced_at: row.last_synced_at
          };
        });

        const flappyList = (topFlappy && topFlappy.results || []).map((row, idx) => ({
          rank: idx + 1,
          device_serial: maskSerial(row.chip_id),
          chip_id: maskSerial(row.chip_id),
          name: row.device_name,
          score: row.flappy_high_score,
          display_score: String(row.flappy_high_score),
          tier: row.flappy_high_score >= 35 ? "Ace Pilot" : "Flyer",
          synced_at: row.last_synced_at
        }));

        const responseData = {
          meta: {
            version: "1.1.0",
            updated_at: new Date().toISOString(),
            total_registered_devices: totalDevices,
            global_boulder_clicks: totalBoulderClicks,
            boulder_milestone_target: targetClicks,
            boulder_progress_pct: Number(((totalBoulderClicks / targetClicks) * 100).toFixed(2))
          },
          applets: {
            sisyphus: {
              id: "sisyphus",
              name: "Sisyphus",
              tags: ["ADHD", "FIDGET"],
              headline: "Every click moves Sisyphus one step up the hill.",
              description: "The boulder always rolls back. But you keep going. A click counter with mythology wrapped around it, turning fidgeting into a story.",
              stat_type: "cumulative_counter",
              primary_metric: "Clicks",
              unit: "clicks",
              sort_direction: "desc",
              community_total: totalBoulderClicks,
              leaderboard: sisyphusList
            },
            just_ten: {
              id: "just_ten",
              name: "Just Ten",
              tags: ["ANXIETY", "BREATHING"],
              headline: "Hold the button for exactly 10 seconds.",
              description: "Looks like a reaction game. The expanding circle quietly syncs your breathing, grounding you mid-stress without anyone noticing.",
              stat_type: "precision_timing",
              primary_metric: "Hold Timing Deviation",
              target_seconds: 10.0,
              unit: "seconds",
              sort_direction: "asc",
              leaderboard: justTenList
            },
            flappy_bird: {
              id: "flappy_bird",
              name: "Flappy Bird",
              tags: ["GAME", "FIDGET"],
              headline: "The classic, on your Click.",
              description: "One button, one bird, infinite pipes. Pure tactile fidget, no touch screen, no ads, no algorithm.",
              stat_type: "high_score",
              primary_metric: "High Score",
              unit: "pipes",
              sort_direction: "desc",
              leaderboard: flappyList
            }
          },
          // Legacy/Direct shortcuts
          global_boulder_clicks: totalBoulderClicks,
          total_devices: totalDevices,
          top_sisyphus: sisyphusList,
          top_just_ten: justTenList,
          top_flappy: flappyList,
          timestamp: new Date().toISOString()
        };

        return new Response(JSON.stringify(responseData), {
          headers: { ...headers, "Content-Type": "application/json" },
        });
      } catch (err) {
        return new Response(JSON.stringify({ error: err.message }), {
          status: 500,
          headers: { ...headers, "Content-Type": "application/json" },
        });
      }
    }

    // POST /api/sync
    if (method === "POST" && url.pathname === "/api/sync") {
      try {
        const db = env.DB;
        if (!db) {
          return new Response(JSON.stringify({ error: "Database not bound" }), {
            status: 500,
            headers: { ...headers, "Content-Type": "application/json" },
          });
        }

        const body = await request.json();
        const rawChipId = body.chip_id || "";
        const chipId = String(rawChipId).replace(/[^A-Za-z0-9]/g, "").toUpperCase();
        if (!chipId || chipId.length < 4 || chipId.length > 32) {
          return new Response(JSON.stringify({ error: "Invalid chip_id" }), {
            status: 400,
            headers: { ...headers, "Content-Type": "application/json" },
          });
        }

        const deviceName = sanitizeText(body.name);
        const clicks = Math.max(0, parseInt(body.clicks, 10) || 0);
        const flappyScore = Math.max(0, parseInt(body.flappy, 10) || 0);
        const justTenScore = Math.max(0, parseInt(body.just_ten, 10) || parseInt(body.just_ten_best_ms, 10) || 0);
        const uptimeHrs = Math.max(0, parseInt(body.uptime_hrs, 10) || 0);

        // Check if device already exists
        const existing = await db.prepare(
          "SELECT chip_id, device_name, total_clicks, last_synced_clicks, last_synced_at, flappy_high_score, just_ten_best_ms FROM devices WHERE chip_id = ?"
        ).bind(chipId).first();

        let creditedDelta = 0;

        if (!existing) {
          // New device: Initial registration cap (max 50k clicks to safeguard global sum)
          creditedDelta = Math.min(clicks, 50000);

          await db.batch([
            db.prepare(
              "INSERT INTO devices (chip_id, device_name, total_clicks, last_synced_clicks, last_synced_at, flappy_high_score, just_ten_best_ms, uptime_hrs) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, ?, ?, ?)"
            ).bind(chipId, deviceName, clicks, clicks, flappyScore, justTenScore, uptimeHrs),
            db.prepare(
              "UPDATE global_stats SET total_boulder_clicks = total_boulder_clicks + ?, total_devices = total_devices + 1, last_updated = CURRENT_TIMESTAMP WHERE id = 1"
            ).bind(creditedDelta),
            db.prepare(
              "INSERT INTO sync_log (chip_id, delta_claimed, delta_credited) VALUES (?, ?, ?)"
            ).bind(chipId, clicks, creditedDelta),
          ]);
        } else {
          // Existing device: calculate delta
          const rawDelta = clicks - (existing.last_synced_clicks || 0);

          if (rawDelta > 0) {
            // Calculate time elapsed in hours since last sync
            const lastTime = new Date(existing.last_synced_at).getTime();
            const nowTime = Date.now();
            const elapsedHours = Math.max(1, (nowTime - lastTime) / (1000 * 60 * 60));
            const elapsedDays = Math.max(1, elapsedHours / 24);

            // Maximum allowed human velocity: 40,000 clicks per day
            const maxAllowed = Math.ceil(elapsedDays * 40000);
            creditedDelta = Math.min(rawDelta, maxAllowed);
          } else {
            creditedDelta = 0;
          }

          const newFlappy = Math.max(existing.flappy_high_score || 0, flappyScore);
          const existingJustTen = existing.just_ten_best_ms || 0;
          let newJustTen = existingJustTen;
          if (justTenScore > 0) {
            newJustTen = (existingJustTen === 0) ? justTenScore : Math.min(existingJustTen, justTenScore);
          }

          await db.batch([
            db.prepare(
              "UPDATE devices SET device_name = ?, total_clicks = ?, last_synced_clicks = ?, last_synced_at = CURRENT_TIMESTAMP, flappy_high_score = ?, just_ten_best_ms = ?, uptime_hrs = MAX(uptime_hrs, ?) WHERE chip_id = ?"
            ).bind(deviceName, clicks, clicks, newFlappy, newJustTen, uptimeHrs, chipId),
            db.prepare(
              "UPDATE global_stats SET total_boulder_clicks = total_boulder_clicks + ?, last_updated = CURRENT_TIMESTAMP WHERE id = 1"
            ).bind(creditedDelta),
            db.prepare(
              "INSERT INTO sync_log (chip_id, delta_claimed, delta_credited) VALUES (?, ?, ?)"
            ).bind(chipId, Math.max(0, rawDelta), creditedDelta),
          ]);
        }

        // Return updated boulder stats
        const updatedGlobal = await db.prepare(
          "SELECT total_boulder_clicks, total_devices FROM global_stats WHERE id = 1"
        ).first();

        return new Response(
          JSON.stringify({
            success: true,
            chip_id: chipId,
            device_name: deviceName,
            credited_delta: creditedDelta,
            total_boulder_clicks: updatedGlobal ? updatedGlobal.total_boulder_clicks : 0,
            message: `Successfully synced! +${creditedDelta.toLocaleString()} clicks added to the Community Boulder.`,
          }),
          { headers: { ...headers, "Content-Type": "application/json" } }
        );
      } catch (err) {
        return new Response(JSON.stringify({ error: err.message }), {
          status: 500,
          headers: { ...headers, "Content-Type": "application/json" },
        });
      }
    }

    return new Response(JSON.stringify({ error: "Endpoint not found" }), {
      status: 404,
      headers: { ...headers, "Content-Type": "application/json" },
    });
  },
};
