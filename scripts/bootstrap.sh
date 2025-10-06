#!/bin/bash
set -e

# SoulScout Bootstrap Script
# Validates configuration, builds images, and starts the complete system

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

echo "============================================"
echo "SoulScout Bootstrap"
echo "============================================"
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Validation function
validate_env() {
    local var_name=$1
    local var_value=$(grep "^${var_name}=" .env 2>/dev/null | cut -d '=' -f2-)
    
    if [ -z "$var_value" ] || [ "$var_value" = "CHANGE_ME_STRONG_PASSWORD_HERE" ] || [[ "$var_value" == *"123456789"* ]]; then
        echo -e "${RED}✗${NC} $var_name not set or using placeholder value"
        return 1
    else
        echo -e "${GREEN}✓${NC} $var_name configured"
        return 0
    fi
}

# Check if .env exists
if [ ! -f .env ]; then
    echo -e "${RED}ERROR: .env file not found${NC}"
    echo "Please copy .env.example to .env and configure it:"
    echo "  cp .env.example .env"
    echo "  nano .env"
    exit 1
fi

echo "Validating configuration..."
echo "-------------------------------------------"

# Validate required environment variables
VALIDATION_FAILED=0

validate_env "TG_BOT_TOKEN" || VALIDATION_FAILED=1
validate_env "OWNER_TELEGRAM_ID" || VALIDATION_FAILED=1
validate_env "POSTGRES_PASSWORD" || VALIDATION_FAILED=1
validate_env "RPC_URLS" || VALIDATION_FAILED=1

if [ $VALIDATION_FAILED -eq 1 ]; then
    echo ""
    echo -e "${RED}Configuration validation failed!${NC}"
    echo "Please edit .env and set all required values:"
    echo "  nano .env"
    exit 1
fi

echo ""
echo -e "${GREEN}Configuration validated successfully!${NC}"
echo ""

# Create required directories
echo "Creating required directories..."
mkdir -p data/pgdata data/redis ops/backups
echo -e "${GREEN}✓${NC} Directories created"
echo ""

# Check if Docker is running
echo "Checking Docker..."
if ! docker info > /dev/null 2>&1; then
    echo -e "${RED}ERROR: Docker is not running${NC}"
    echo "Please start Docker and try again"
    exit 1
fi
echo -e "${GREEN}✓${NC} Docker is running"
echo ""

# Check if Docker Compose is available
echo "Checking Docker Compose..."
if ! docker compose version > /dev/null 2>&1; then
    echo -e "${RED}ERROR: Docker Compose is not available${NC}"
    echo "Please install Docker Compose and try again"
    exit 1
fi
echo -e "${GREEN}✓${NC} Docker Compose is available"
echo ""

# Stop any existing containers
echo "Stopping existing containers (if any)..."
docker compose down > /dev/null 2>&1 || true
echo -e "${GREEN}✓${NC} Cleanup complete"
echo ""

# Build or pull images
echo "Building Docker images..."
echo "This may take 5-15 minutes on first run..."
echo "-------------------------------------------"
docker compose build

echo ""
echo -e "${GREEN}✓${NC} Images built successfully"
echo ""

# Start services with health checks
echo "Starting services..."
echo "-------------------------------------------"
docker compose up -d

echo ""
echo "Waiting for services to become healthy..."
echo "(This may take 1-2 minutes)"
echo ""

# Wait for all services to be healthy
MAX_WAIT=180  # 3 minutes
ELAPSED=0
INTERVAL=5

while [ $ELAPSED -lt $MAX_WAIT ]; do
    HEALTHY_COUNT=$(docker compose ps --format json | jq -r '.Health' | grep -c "healthy" || echo "0")
    TOTAL_COUNT=$(docker compose ps --format json | wc -l)
    
    echo -e "${YELLOW}⏳${NC} Healthy: $HEALTHY_COUNT/$TOTAL_COUNT services"
    
    if [ "$HEALTHY_COUNT" -eq "$TOTAL_COUNT" ]; then
        echo ""
        echo -e "${GREEN}✓${NC} All services are healthy!"
        break
    fi
    
    sleep $INTERVAL
    ELAPSED=$((ELAPSED + INTERVAL))
done

if [ $ELAPSED -ge $MAX_WAIT ]; then
    echo ""
    echo -e "${YELLOW}WARNING: Not all services became healthy within timeout${NC}"
    echo "Checking individual service status:"
    docker compose ps
    echo ""
    echo "View logs with: docker compose logs -f"
fi

echo ""
echo "============================================"
echo "Service Status"
echo "============================================"
docker compose ps

echo ""
echo "============================================"
echo "Health Check Endpoints"
echo "============================================"
echo "Gateway:   http://localhost:8080/health"
echo "Portfolio: http://localhost:8081/health"
echo "Ingestor:  http://localhost:8082/health"
echo "Analytics: http://localhost:8083/health"
echo "Notifier:  http://localhost:8084/health"

echo ""
echo "============================================"
echo "Next Steps"
echo "============================================"
echo "1. Run smoke tests:"
echo "   ./scripts/smoke.sh"
echo ""
echo "2. View all logs:"
echo "   docker compose logs -f"
echo ""
echo "3. View specific service logs:"
echo "   docker compose logs -f tg_gateway"
echo ""
echo "4. Test with Telegram:"
echo "   Open your bot in Telegram and send /start"
echo ""
echo -e "${GREEN}✓ Bootstrap complete!${NC}"