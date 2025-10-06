# SoulScout Analytics Engine v1.1

The Analytics Engine is the intelligence core of SoulScout. It implements the complete v1.1 signal specification, confidence scoring, risk regime detection, entry/exit logic, and alert generation.

## Architecture

```
Ingestor → Redis (market.updates) → Analytics Engine → Redis (alerts) → Notifier
                                          ↓
                                      Postgres
```

## v1.1 Signal Specification

### Hard Gates (Must Pass for Actionable)
- **S1**: Liquidity USD ≥ 150k (25k-150k → Heads-up only)
- **S2**: Vol24h USD ≥ 500k (50k-500k → Heads-up only)
- **S8**: Spread ≤ 2.5%, Impact@1% ≤ 1.5%
- **S10**: Route to USDC/USDT ≤ 3 hops, deviation < 0.8%

### Confidence Signals (S1-S10)
1. **S1**: Liquidity gates
2. **S2**: Volume gates
3. **S3**: FDV/Liq ratio (preferred 5-50)
4. **S4**: Momentum (m1h +1..+12%, m24h +2..+60%)
5. **S5**: Drawdown structure (higher lows)
6. **S6**: Volatility/consistency
7. **S7**: Rug risk heuristics
8. **S8**: Execution cost
9. **S9**: Volume trend
10. **S10**: Route health

### Hygiene Check
- **N1**: Token list hygiene (-10C if not on mirrored lists)

### Weighted Confidence Score

```
R = Σ (Si × wi)  # Raw score (0-100)
DQ = data_quality_factor (0.7-1.0)
P = penalties (age, FDV/Liq, etc.)
C = max(0, R - P)  # Final confidence
```

**v1.1 Weights:**
- S1: 15%, S2: 12%, S3: 8%, S4: 18%
- S5: 10%, S6: 8%, S7: 12%, S8: 10%
- S9: 5%, S10: 2%

### Age and Risk Rules
- **Age floor**: 24h minimum
- **Young-and-risky**: Age <72h + authorities unknown → requires C ≥ 80 for Actionable
- **Rug cap**: S7 <0.3 → cap C at 55

### Entry Confirmation
Required if m1h > +12%:
1. **Retest/hold**: 5m close back above prior breakout, OR
2. **Quick pullback**: 2-5% pullback, then 15m VWAP-proxy close above

### Net Edge Check
Must have U ≥ 2×K where:
- **U** = upside to 24h swing high (capped at 15%)
- **K** = spread + impact + lag (0.30%)

If fails → force Heads-up instead of Actionable

### Risk Regime Filter
Inputs: SOL 24h return, median 24h return, share above VWAP

**Risk-On** (2+ positive):
- Threshold: -10 (60 instead of 70)
- Size: +30%

**Risk-Off** (2+ negative):
- Threshold: +10 (80 instead of 70)
- Size: -30%

### Alert Bands
- **Heads-up**: 60-69
- **Actionable**: ≥ regime-adjusted threshold
- **High Conviction**: ≥ 85, no hard risk flags

### Throttles & Cooldowns
- **Per-token cooldown**: 6h Actionable, 1h Heads-up
- **Global throttle**: Max 5 Actionable/hour
- **Dedup**: Hash reasons, block identical within TTL
- **Re-entry guard**: No re-entry 12h post-stop unless High-conviction ≥85

### Position Sizing (Advisory)
- ATR cap: ~0.6% wallet_SOL risk per 1× ATR(1h)
- Liquidity cap: Size_USD ≤ 0.008 × LiquidityUSD
- Global caps: ≤3 positions, ≤35% deployed, keep 5-10% SOL free

### Exit Templates
- Trim 25% at +15%, 25% at +30%
- Trail rest by 15m higher-low or 10% trailing stop
- Time stop if <+5% in 24h
- Structure-fail cut

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `REDIS_URL` | `redis://localhost:6379` | Redis connection |
| `PG_DSN` | *required* | Postgres connection |
| `STREAM_MARKET` | `soul.market.updates` | Market data input |
| `STREAM_ALERTS` | `soul.alerts` | Alert output |
| `ACTIONABLE_BASE_THRESHOLD` | `70` | Base confidence for Actionable |
| `RISK_ON_ADJ` | `-10` | Risk-on threshold adjustment |
| `RISK_OFF_ADJ` | `10` | Risk-off threshold adjustment |
| `GLOBAL_ACTIONABLE_MAX_PER_HOUR` | `5` | Max Actionable alerts/hour |
| `COOLDOWN_ACTIONABLE_HOURS` | `6` | Actionable cooldown |
| `COOLDOWN_HEADSUP_HOURS` | `1` | Heads-up cooldown |
| `REENTRY_GUARD_HOURS` | `12` | Re-entry guard period |
| `LISTEN_PORT` | `8083` | Health endpoint port |
| `LOG_LEVEL` | `info` | Logging level |

## Alert Schema

```json
{
  "severity": "heads_up|actionable|high_conviction",
  "symbol": "TICKER",
  "price": 0.083,
  "confidence": 78,
  "lines": [
    "Liq $310k; Vol24h $1.2M; m1h +6.2%; m24h +18%",
    "Retest/hold confirmed",
    "FDV/Liq est; Age 102h",
    "Route 2 hops dev 0.3%"
  ],
  "plan": "Trim 25% at +15; 25% at +30; trail rest",
  "sol_path": "TICKER → USDC → SOL",
  "est_impact_pct": 0.7,
  "ts": "2025-10-05T14:23:00Z"
}
```

## Decision Flow

```
1. Consume market update from Redis
2. Update token state (24h rolling window)
3. Compute signals S1-S10, N1
4. Check hard gates (S1, S2, S8, S10)
5. Check age floor (24h minimum)
6. Compute confidence score C
7. Check entry confirmation (if m1h >12%)
8. Net edge check (U ≥ 2×K)
9. Risk regime assessment
10. Determine band
11. Apply throttles/cooldowns
12. Build alert & publish
```

## Building

```bash
docker build -t soulscout/analytics .
```

## Running

```bash
docker run -d \
  --name analytics \
  -e REDIS_URL="redis://redis:6379" \
  -e PG_DSN="postgresql://user:pass@postgres:5432/soulsct" \
  -p 8083:8083 \
  soulscout/analytics
```

## Testing

```bash
cd build
ctest --verbose
```

Tests cover:
- v1.1 signal calculation
- Confidence scoring with penalties
- Hard gates (liquidity, volume, spread, route)
- Entry confirmation (retest/hold, pullback)
- Net edge check
- Throttles and cooldowns
- Risk regime detection

## Monitoring

Key metrics to watch:
- Alerts generated per hour
- Confidence distribution (how many 60-69, 70-84, 85+)
- Gate failures (which S1-S10 most commonly fail)
- Cooldown hits (tokens trying to re-alert)
- Risk regime switches

## Logging

Structured logs include:
- Symbol and confidence for every decision
- Gate failures with specific values
- Entry confirmation status
- Net edge calculations
- Regime adjustments
- Throttle/cooldown blocks

## Health Endpoint

```bash
GET http://localhost:8083/health
```

Response:
```json
{
  "ok": true,
  "redis": true,
  "postgres": true,
  "loop": "running",
  "last_decision_ts": "2025-10-05T14:23:00Z"
}
```

## Troubleshooting

### No alerts generated
- Check if market data is flowing (STREAM_MARKET)
- Verify hard gates aren't blocking everything
- Lower ACTIONABLE_BASE_THRESHOLD temporarily for testing

### Too many alerts
- Increase cooldown periods
- Tighten hard gates (raise S1, S2 minimums)
- Reduce GLOBAL_ACTIONABLE_MAX_PER_HOUR

### Alerts for risky tokens
- Review S7 rug risk logic
- Enable young-and-risky C≥80 requirement
- Check DQ forced Heads-up is working

## License

Part of the SoulScout system.