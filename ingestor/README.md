# SoulScout Market Ingestor

The Market Ingestor continuously polls Solana DEX endpoints (Raydium, Orca), normalizes pool data, synthesizes OHLCV bars, stores metrics in Postgres, and publishes real-time updates to Redis Streams for the Analytics Engine.

## Features

- **Multi-DEX Support**: Raydium and Orca pool monitoring
- **Real-time Bar Synthesis**: 5-minute and 15-minute OHLCV bars
- **Impact Modeling**: XYK-based price impact estimation for 1% trades
- **Graceful Degradation**: Marks data quality when reconstructing sparse data
- **Rate Limit Aware**: Exponential backoff with jitter on 429/5xx responses
- **Idempotent Writes**: Upsert-based Postgres writes prevent duplicates

## Architecture

```
DEX APIs (Raydium/Orca) → Normalize → Synthesize Bars → Store
                                                          ↓
                                          Redis Stream → Analytics
                                          Postgres ← Historical data
```

## Data Flow

1. **Poll Tick** (every 60s by default):
   - Fetch active pools from Raydium and Orca
   - Query Jupiter for route health (USDC/USDT paths)
   
2. **Normalize**:
   - Standardize pool data across DEXes
   - Compute spread and 1% impact estimates
   - Mark degraded data quality
   
3. **Synthesize**:
   - Maintain rolling 5m and 15m bar windows
   - Emit completed bars on interval boundaries
   
4. **Persist**:
   - Write 5m stats to `pool_stats_5m`
   - Write 15m bars to `pool_stats_15m`
   - Track token first liquidity events
   
5. **Publish**:
   - Send normalized updates to `soul.market.updates`
   - Analytics consumes for signal generation

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `REDIS_URL` | `redis://localhost:6379` | Redis connection |
| `STREAM_MARKET` | `soul.market.updates` | Market update stream |
| `PG_DSN` | *required* | Postgres connection string |
| `RPC_URLS` | *required* | Comma-separated Solana RPC URLs |
| `RAYDIUM_BASE` | `https://api.raydium.io/v2` | Raydium API |
| `ORCA_BASE` | `https://api.orca.so` | Orca API |
| `JUPITER_BASE` | `https://quote-api.jup.ag/v6` | Jupiter API |
| `MAX_CONCURRENCY` | `8` | Thread pool size |
| `REQUEST_TIMEOUT_MS` | `8000` | HTTP request timeout |
| `GLOBAL_TICK_SECONDS` | `60` | Poll interval |
| `BAR_INTERVAL_5M` | `300` | 5-minute bar interval |
| `BAR_INTERVAL_15M` | `900` | 15-minute bar interval |
| `RETRY_BACKOFF_MS_MIN` | `500` | Min backoff on error |
| `RETRY_BACKOFF_MS_MAX` | `15000` | Max backoff on error |
| `LISTEN_PORT` | `8082` | Health endpoint port |
| `LOG_LEVEL` | `info` | Logging level |

## Market Update Schema

Published to `soul.market.updates`:

```json
{
  "pool": "pool_address",
  "mint_base": "base_token_mint",
  "mint_quote": "quote_token_mint",
  "price": 0.083,
  "liq_usd": 310000,
  "vol24h_usd": 1200000,
  "spread_pct": 0.45,
  "impact_1pct_pct": 0.9,
  "age_hours": 102.5,
  "route": {
    "ok": true,
    "hops": 2,
    "dev_pct": 0.3
  },
  "bars": {
    "5m": {"o": 0.081, "h": 0.085, "l": 0.079, "c": 0.083, "v_usd": 51000},
    "15m": {"o": 0.080, "h": 0.086, "l": 0.079, "c": 0.083, "v_usd": 140000}
  },
  "dq": "ok",
  "ts": "2025-10-05T14:23:00Z"
}
```

## Database Schema

### `pools`
- `id` - Pool identifier
- `address` - Pool address (unique)
- `mint_base` - Base token mint
- `mint_quote` - Quote token mint
- `dex` - Source DEX (raydium/orca)
- `first_seen` - First observation timestamp

### `pool_stats_5m`
- `pool_id` - Foreign key to pools
- `ts` - 5-minute interval timestamp
- `price`, `liq_usd`, `vol24h_usd` - Metrics
- `spread_pct`, `impact_1pct_pct` - Estimated costs
- `route_ok`, `route_hops`, `route_dev_pct` - Route health
- `dq` - Data quality ("ok" or "degraded")

### `pool_stats_15m`
- `pool_id` - Foreign key to pools
- `ts` - 15-minute interval timestamp
- `o`, `h`, `l`, `c` - OHLC prices
- `v_usd` - Volume in USD

### `token_first_liq`
- `mint` - Token mint (primary key)
- `first_liq_ts` - First time liquidity crossed 25k
- `first_pool_id` - Pool where threshold crossed

## Graceful Degradation

Data quality flag (`dq`) is set to "degraded" when:
- Missing key pool metrics (price = 0, liquidity = 0)
- Bar synthesized from < 3 ticks
- Reconstructed volume from sparse data
- Failed to fetch route information

Analytics Engine uses `dq` flag to apply penalties in confidence scoring.

## Rate Limiting & Backoff

- Detects HTTP 429 and 5xx responses
- Applies exponential backoff: `random(500ms, 15000ms)`
- Rotates through multiple RPC endpoints on failure
- Logs backoff events for monitoring

## Building

```bash
# Local
export VCPKG_ROOT=/path/to/vcpkg
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Docker
docker build -t soulscout/ingestor .
```

## Running

```bash
docker run -d \
  --name ingestor \
  -e REDIS_URL="redis://redis:6379" \
  -e PG_DSN="postgresql://user:pass@postgres:5432/soulsct" \
  -e RPC_URLS="https://api.mainnet-beta.solana.com" \
  -p 8082:8082 \
  soulscout/ingestor
```

## Health Endpoint

```bash
GET http://localhost:8082/health
```

Response:
```json
{
  "ok": true,
  "redis": true,
  "postgres": true,
  "rpc": "up",
  "dex": {
    "raydium": "up",
    "orca": "up"
  },
  "jupiter": "up"
}
```

## Testing

```bash
cd build
ctest --verbose
```

Tests cover:
- Bar synthesis (OHLCV computation from ticks)
- Impact model (XYK 1% impact calculation)
- Pool normalization (DEX data standardization)
- Store idempotency (duplicate tick handling)

## Performance

- **Tick Duration**: Typically 2-5 seconds for 100 pools
- **Memory**: ~200MB for 1000 tracked pools
- **Database**: ~10MB/day for 100 pools with 5m granularity
- **Redis Stream**: Lightweight updates (~500 bytes each)

## Monitoring

Watch for:
- `dex.*.status` in health endpoint (track API availability)
- Log entries with `"backing off"` (rate limit hits)
- `dq="degraded"` ratio in market updates (data quality)
- Pool count trends (new pools discovered)

## Troubleshooting

### No pools fetched
- Verify DEX API endpoints are accessible
- Check logs for HTTP errors
- Ensure RPC endpoints are responsive

### High degraded data ratio
- May indicate sparse market data periods
- Check DEX API response completeness
- Verify Jupiter routing is working

### Database write errors
- Check Postgres connection and permissions
- Verify schema was initialized
- Look for constraint violations in logs

## License

Part of the SoulScout system.
