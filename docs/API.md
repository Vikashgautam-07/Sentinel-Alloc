# API Documentation

## Base URL
```
http://localhost:8080
```

## Endpoints

### 1. Get Heap Status
Get current heap statistics and all blocks.

**Request:**
```
GET /api/status
```

**Response:**
```json
{
  "heap_status": {
    "total_size": 4096,
    "allocated": 1024,
    "free": 3072,
    "num_allocations": 2,
    "num_blocks": 3,
    "fragmentation": "33%",
    "failed_allocations": 0
  },
  "blocks": [
    {
      "size": 512,
      "is_free": false,
      "address": "0x56557080"
    },
    {
      "size": 512,
      "is_free": false,
      "address": "0x56557280"
    },
    {
      "size": 3072,
      "is_free": true,
      "address": "0x56557480"
    }
  ]
}
```

**Status Codes:**
- `200` - Success
- `500` - Server error

---

### 2. Allocate Memory
Allocate memory from the heap.

**Request:**
```
POST /api/malloc
Content-Type: application/json

{
  "size": 256
}
```

**Response:**
```json
{
  "success": true,
  "address": "0x56557080",
  "size": 256
}
```

Or on failure:
```json
{
  "success": false,
  "error": "Allocation failed - insufficient memory"
}
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| size | integer | Number of bytes to allocate (1-4096) |

**Status Codes:**
- `200` - Success or allocation failed (check success field)
- `400` - Invalid parameters

**Notes:**
- Returns pointer address on success
- Address is valid until freed
- Returns same address for multiple allocations of same size

---

### 3. Free Memory
Release allocated memory back to heap.

**Request:**
```
POST /api/free
Content-Type: application/json

{
  "address": "0x56557080"
}
```

**Response:**
```json
{
  "success": true
}
```

Or on failure:
```json
{
  "success": false,
  "error": "Invalid pointer"
}
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| address | string | Pointer address (hex format) |

**Status Codes:**
- `200` - Success or error (check success field)
- `400` - Invalid address format

**Notes:**
- Triggers automatic coalescing
- Double-free attempts are caught
- Invalid pointers are rejected

---

### 4. Reset Heap
Clear all allocations and reset heap to initial state.

**Request:**
```
POST /api/reset
```

**Response:**
```json
{
  "success": true,
  "message": "Heap reset"
}
```

**Status Codes:**
- `200` - Success

**Notes:**
- All previous allocations become invalid
- Heap returns to single free block
- Confirmation recommended in UI

---

### 5. Verify Heap Integrity
Validate heap data structure consistency.

**Request:**
```
POST /api/verify
```

**Response:**
```json
{
  "valid": true
}
```

Or if invalid:
```json
{
  "valid": false
}
```

**Status Codes:**
- `200` - Success

**Notes:**
- Checks pointer validity
- Validates block sizes
- Detects corruption
- Non-intrusive (read-only)

---

## Error Responses

### 400 Bad Request
```json
{
  "error": "Invalid size"
}
```

### 404 Not Found
```json
{
  "error": "Endpoint not found"
}
```

### 500 Internal Server Error
```json
{
  "error": "Server error"
}
```

---

## Data Types

### Block Object
```json
{
  "size": 256,
  "is_free": false,
  "address": "0x56557080"
}
```

### HeapStatus Object
```json
{
  "total_size": 4096,
  "allocated": 1024,
  "free": 3072,
  "num_allocations": 2,
  "num_blocks": 3,
  "fragmentation": "33%",
  "failed_allocations": 0
}
```

---

## CORS Support

All endpoints support Cross-Origin Resource Sharing (CORS):

```
Access-Control-Allow-Origin: *
```

This enables requests from any origin.

---

## Rate Limiting

Currently no rate limiting is implemented. Production deployments should add:
- Per-IP rate limiting
- Request authentication
- Connection pooling

---

## Example Workflows

### Complete Allocation Cycle

```bash
# 1. Allocate 256 bytes
curl -X POST http://localhost:8080/api/malloc \
  -H "Content-Type: application/json" \
  -d '{"size": 256}'

# Response: {"success": true, "address": "0x56557080", "size": 256}

# 2. Check heap status
curl http://localhost:8080/api/status

# 3. Free the allocation
curl -X POST http://localhost:8080/api/free \
  -H "Content-Type: application/json" \
  -d '{"address": "0x56557080"}'

# Response: {"success": true}

# 4. Verify integrity
curl -X POST http://localhost:8080/api/verify

# Response: {"valid": true}
```

### Stress Testing

```bash
# Allocate multiple blocks
for i in {1..10}; do
  curl -X POST http://localhost:8080/api/malloc \
    -H "Content-Type: application/json" \
    -d '{"size": 256}'
done

# Check utilization
curl http://localhost:8080/api/status | jq '.heap_status.utilization'

# Reset
curl -X POST http://localhost:8080/api/reset
```

---

## Performance

### Latency Characteristics
- **GET /api/status**: ~1-5ms (O(n) blocks)
- **POST /api/malloc**: ~1-10ms (depends on strategy)
- **POST /api/free**: ~2-15ms (includes coalescing)
- **POST /api/reset**: <1ms
- **POST /api/verify**: ~1-5ms

### Throughput
- Handles ~100-200 requests/sec per thread
- Multi-threaded server supports concurrent clients
- Memory allocation thread-safe with mutex protection

---

## Versioning

Current API version: **1.0**

Future versions may add:
- Batch operations
- Statistical endpoints
- Performance metrics
- Advanced filtering

---

## Security Considerations

1. **No Authentication** - Add API keys in production
2. **No Input Validation** - Validate all inputs
3. **Pointer Validation** - Check bounds before use
4. **No Logging** - Implement audit logging
5. **No Rate Limiting** - Add DDoS protection

---

## Troubleshooting

### Connection Refused
```
Error: Connection refused
Solution: Ensure server is running on port 8080
```

### Invalid Address
```json
{
  "success": false,
  "error": "Invalid pointer"
}
```
Solution: Use address from previous allocation response

### Out of Memory
```json
{
  "success": false,
  "error": "Allocation failed - insufficient memory"
}
```
Solution: Free unused allocations or reset heap

---

## References
- See [README.md](../README.md) for overview
- See [ARCHITECTURE.md](./ARCHITECTURE.md) for design details
- See [WEB_GUI.md](./WEB_GUI.md) for GUI documentation
