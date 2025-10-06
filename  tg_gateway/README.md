# SoulScout Telegram Gateway

The Telegram Gateway is the entry point for the SoulScout system. It handles all Telegram bot interactions, enforces access control, and routes commands to backend services via Redis Streams.

## Features

- **Dual Mode Operation**: Supports both polling and webhook modes
- **Access Control**: Owner-only control with optional time-limited guest access
- **Rate Limiting**: Per-user rate limiting (default 20 msgs/min)
- **Command Routing**: Publishes commands to Redis, consumes replies
- **Alert Forwarding**: Consumes formatted alerts from Notifier, forwards to Telegram
- **Audit Logging**: All authentication events logged to Redis

## Architecture

```
Telegram User
     ↓
TG Gateway (poll or webhook)
     ↓
Redis Streams ← → Other Services
     ↓
Responses back to Telegram
```

## Supported Commands

### All Users
- `/start` - Initialize bot (guests use `/start <PIN>`)
- `/help` - Show available commands

### Owner & Guests
- `/balance` - View portfolio balance
- `/holdings [limit]` - View top holdings
- `/signals [window]` - View trading signals (e.g., `/signals 24h`)
- `/health` - System status

### Owner Only
- `/silence [minutes]` - Mute alerts (default 30 min)
- `/resume` - Resume alerts
- `/add_wallet <address>` - Track additional wallet
- `/remove_wallet <address>` - Stop tracking wallet
- `/guest [minutes]` - Generate time-limited guest PIN

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TG_BOT_TOKEN` | *required* | Telegram bot token from @BotFather |
| `OWNER_TELEGRAM_ID` | *required* | Your Telegram user ID |
| `REDIS_URL` | `redis://localhost:6379` | Redis connection string |
| `GATEWAY_MODE` | `poll` | `poll` or `webhook` |
| `WEBHOOK_PUBLIC_URL` | - | Required if mode=webhook |
| `LISTEN_ADDR` | `0.0.0.0` | HTTP server bind address |
| `LISTEN_PORT` | `8080` | HTTP server port |
| `RATE_LIMIT_MSGS_PER_MIN` | `20` | Per-user rate limit |
| `GUEST_DEFAULT_MINUTES` | `30` | Default guest session duration |
| `STREAM_REQ` | `soul.cmd.requests` | Command request stream |
| `STREAM_REP` | `soul.cmd.replies` | Command reply stream |
| `STREAM_ALERTS` | `soul.outbound.alerts` | Alert stream (from Notifier) |
| `STREAM_AUDIT` | `soul.audit` | Audit event stream |
| `SERVICE_NAME` | `tg_gateway` | Service identifier |
| `LOG_LEVEL` | `info` | `debug`, `info`, `warn`, `error` |

## Redis Streams

### Produces
- **soul.cmd.requests**: Command requests to backend services
- **soul.audit**: Authentication and command usage events

### Consumes
- **soul.cmd.replies**: Responses from backend services
- **soul.outbound.alerts**: Formatted alerts from Notifier

## Building

### Local Development
```bash
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh

# Build
export VCPKG_ROOT=/path/to/vcpkg
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Run tests
cd build && ctest
```

### Docker
```bash
docker build -t soulscout/tg_gateway .
```

## Running

### Standalone (for testing)
```bash
export TG_BOT_TOKEN="your-token"
export OWNER_TELEGRAM_ID="123456789"
export REDIS_URL="redis://localhost:6379"
export GATEWAY_MODE="poll"

./build/tg_gateway
```

### Docker
```bash
docker run -d \
  --name tg_gateway \
  -e TG_BOT_TOKEN="your-token" \
  -e OWNER_TELEGRAM_ID="123456789" \
  -e REDIS_URL="redis://redis:6379" \
  -e GATEWAY_MODE="poll" \
  -p 8080:8080 \
  soulscout/tg_gateway
```

### Docker Compose
See main `docker-compose.yml` in project root.

## Webhook Mode

For production deployments with high message volume:

1. Set up a domain with HTTPS (use Caddy or Nginx with Let's Encrypt)
2. Configure environment:
   ```bash
   GATEWAY_MODE=webhook
   WEBHOOK_PUBLIC_URL=https://your-domain.com/telegram/webhook
   ```
3. Ensure port 443 is accessible
4. Gateway will automatically register the webhook with Telegram

## Health Endpoint

```bash
GET http://localhost:8080/health
```

Response:
```json
{
  "ok": true,
  "redis": true,
  "mode": "poll"
}
```

## Guest Access Flow

1. Owner sends `/guest 60` (60 minutes)
2. Bot replies with 6-digit PIN and expiry
3. Owner shares PIN with guest
4. Guest sends `/start <PIN>` in their own chat with the bot
5. Guest gets read-only access for specified duration
6. All guest actions are audit-logged

## Testing

```bash
cd build
ctest --verbose
```

Tests cover:
- Command parsing (all supported commands)
- Authentication (owner, guest, unknown)
- Rate limiting
- Message round-trip (request/reply via Redis)

## Security Notes

- Never commit `TG_BOT_TOKEN` or real `OWNER_TELEGRAM_ID` to version control
- Use environment variables or secrets management
- Guest PINs are single-use and time-limited
- All authentication events are audit-logged
- Rate limiting prevents spam/abuse

## Troubleshooting

### Bot doesn't respond
- Check `TG_BOT_TOKEN` is correct
- Verify Redis is reachable: `redis-cli ping`
- Check logs: `docker logs tg_gateway`
- Ensure `/start` was sent to initialize chat

### "Access denied" for owner
- Verify `OWNER_TELEGRAM_ID` matches your actual Telegram user ID
- Get your ID from @userinfobot on Telegram

### Webhook mode not working
- Ensure `WEBHOOK_PUBLIC_URL` is accessible from internet
- Check SSL certificate is valid
- Verify webhook registration: check Telegram API response in logs
- Port 443 must be open

### High CPU usage in poll mode
- This is normal; polling checks every 30 seconds
- Consider webhook mode for production

## Development Notes

- Built with C++17, CMake, vcpkg
- Dependencies: httplib, redis++, nlohmann_json, curl, spdlog, catch2
- Thread-safe: uses mutexes for shared state
- Graceful shutdown on SIGTERM/SIGINT
- All Redis operations use proper error handling

## License

Part of the SoulScout system.
