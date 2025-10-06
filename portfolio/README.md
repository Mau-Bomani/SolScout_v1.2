# SoulScout Portfolio Service

The Portfolio Service manages wallet tracking, queries Solana RPC for token balances, values holdings using a multi-tier pricing strategy, and stores snapshots in Postgres.

## Features

- **Multi-Wallet Tracking**: Owner can track multiple Solana wallets
- **Automatic Token Discovery**: Fetches all SPL token balances via Solana RPC
- **Multi-Tier Pricing**:
  1. CoinGecko (listed tokens)
  2. DEX pools with ≥75k liquidity
  3. DEX pools 25k-75k with 50% haircut
  4. Unpriced (excluded from totals)
- **Postgres Persistence**: Snapshots, holdings, metadata
- **Redis Streams**: Command/reply pattern for Telegram integration

## Architecture

```
Telegram → Gateway → Redis Streams
                          ↓
                    Portfolio Service
                          ↓
        Solana RPC + CoinGecko + DEX APIs
                          ↓
                      Postgres
```

## Supported Commands

- `/balance` - Show total USD value and summary
- `/holdings [limit]` - List top N holdings by value
- `/add_wallet <address>` - Track a new wallet (owner only)
- `/remove_wallet <address>` - Stop tracking wallet (owner only)

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `OWNER_TELEGRAM_ID` | *required* | Owner's Telegram user ID |
| `REDIS_URL` | `redis://localhost:6379` | Redis connection |
| `PG_DSN` | *required* | Postgres connection string |
| `RPC_URLS` | *required* | Comma-separated Solana RPC endpoints |
| `COINGECKO_BASE` | `https://api.coingecko.com/api/v3` | CoinGecko API |
| `JUPITER_BASE` | `https://quote-api.jup.ag/v6` | Jupiter aggregator |
| `DUST_MIN_USD` | `0.50` | Minimum value to include |
| `HAIRCUT_LOW_LIQ_PCT` | `50` | Haircut for low liquidity |
| `REQUEST_TIMEOUT_MS` | `8000` | External API timeout |
| `LISTEN_PORT` | `8081` | HTTP health endpoint port |
| `LOG_LEVEL` | `info` | Logging level |

## Valuation Policy

Holdings are valued using this priority:

1. **CoinGecko**: If token is listed, use USD price (tag: `CG`)
2. **DEX (≥75k liq)**: Pool mid-price via USDC/USDT route (tag: `DEX`)
3. **DEX (25k-75k liq)**: 50% haircut, shown separately (tag: `EST_50`)
4. **Unpriced**: Excluded from totals (tag: `NA`)

**Output Format:**
```
💼 Portfolio Balance

Total: $12,345.67 USD
Assets: 8 included

Excludes 3 unpriced tokens. Haircut subtotal: $234.56
```

## Database Schema

### Tables
- `users` - Telegram users (owner/guest)
- `wallets` - Tracked Solana addresses
- `token_metadata` - Cached token info (symbol, decimals)
- `price_points` - Historical pricing data
- `portfolio_snapshots` - Balance snapshots over time
- `holding_values` - Per-token breakdown for each snapshot
- `audits` - Wallet add/remove events

## Building

```bash
# Local build
export VCPKG_ROOT=/path/to/vcpkg
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Docker
docker build -t soulscout/portfolio .
```

## Running

### Standalone
```bash
export OWNER_TELEGRAM_ID="123456789"
export REDIS_URL="redis://localhost:6379"
export PG_DSN="postgresql://user:pass@localhost:5432/soulsct"
export RPC_URLS="https://api.mainnet-beta.solana.com"

./build/portfolio
```

### Docker Compose
See main `docker-compose.yml` in project root.

## Health Endpoint

```bash
GET http://localhost:8081/health
```

Response:
```json
{
  "ok": true,
  "redis": true,
  "postgres": true,
  "rpc": "up"
}
```

## Usage Example

```
User: /add_wallet 7xKXtg2CW87d97TXJSDpbD5jBkheTqA83TZRuJosgAsU
Bot: ✅ Wallet added: 7xKXtg2C...

User: /balance
Bot: 💼 Portfolio Balance

Total: $1,234.56 USD
Assets: 5 included

Excludes 2 unpriced tokens.
Updated: 2025-10-05 14:23

User: /holdings
Bot: 📊 Top Holdings

1. SOL - 12.5000 ($2,500.00)
2. USDC - 500.0000 ($500.00)
3. BONK - 1000000.0000 ($234.56)

+ 2 more...
```

## Data Sources (Free Tier)

- **Solana RPC**: Public endpoints for token account queries
- **CoinGecko**: Free tier for major token prices
- **Raydium/Orca**: Public pool endpoints for liquidity data
- **Jupiter**: Quote/route API for price discovery

All queries respect rate limits with automatic retry and backoff.

## Testing

```bash
cd build
ctest --verbose
```

Tests cover:
- Valuation logic (dust filtering, haircut, totals)
- Wallet command validation
- Price oracle priority ordering
- Postgres round-trip (snapshot save/load)

## Security

- Never logs private keys (service never sees them)
- Redacts passwords in connection strings
- Owner-only commands enforced
- All wallet operations audit-logged

## Troubleshooting

### "No wallets configured"
- Owner must first run `/add_wallet <address>`
- Verify wallet address is valid Base58

### RPC timeouts
- Check `RPC_URLS` points to accessible endpoints
- Service rotates through multiple RPCs on failure
- Monitor logs for `rpc_ok` status in health checks

### Price data missing
- Free tier APIs have rate limits
- CoinGecko may not list all tokens
- DEX data depends on pool availability
- Check `valuation_tag` in holdings for why token wasn't priced

### High database growth
- Consider retention policy (see ops/retention.sql)
- Archive old snapshots periodically
- Prune holding_values for old snapshots

## Performance Notes

- First `/balance` query may take 5-10 seconds (fetching all token accounts)
- Subsequent queries benefit from metadata cache
- RPC queries are the primary bottleneck
- Consider caching token balances for high-frequency queries

## License

Part of the SoulScout system.