# Login Server — Client API Reference

**Transport:** TCP / JSON, delimiter `\n`  
**Max message size:** 8 KB  
**Host/Port:** configured in `config.json` (default `27014`)

---

## Common Packet Structure

### Request (Client → Server)
```json
{
  "header": {
    "eventType": "<eventType>",
    "clientId": 0,
    "hash": ""
  },
  "body": {}
}
```

### Response (Server → Client)
```json
{
  "header": {
    "eventType": "<eventType>",
    "status": "success | error",
    "message": "Human-readable message or error code",
    "clientId": 0,
    "hash": ""
  },
  "body": {}
}
```

`clientId` and `hash` are **0 / empty** on the very first request. After successful login or registration the client must store and send them with every subsequent request.

Every `status:error` response additionally carries `body.errorCode`
mirroring `header.message` (machine-readable; see protocol `00`, error
handling). Match on `body.errorCode`, never on human text.

---

## 0. Client version gate (login only!)

`registerAccount` and `authentificationClient` must send
`body.clientVersion` (`"major.minor.patch"`). Outside
`[min_client_version, max_client_version]` (dev: `0.1.2–0.1.4`; config.json
+ `MIN/MAX_CLIENT_VERSION` env) the server rejects **before** auth:

| Condition | `message` / `errorCode` |
|-----------|--------------------------|
| Below min, missing or garbled | `ERR_VERSION_OUTDATED` |
| Above max | `ERR_VERSION_TOO_NEW` |

Reject body additionally carries `serverVersion` (`"1.0"`),
`minClientVersion`, `maxClientVersion` — show them in the
"update required" UI. **Game/chunk never check versions** (old and future
clients keep working there).

---

## 1. Register Account

**Request eventType:** `registerAccount`  
**Auth required:** No

### Request
```json
{
  "header": { "eventType": "registerAccount" },
  "body": {
    "login":    "MyLogin",
    "password": "MyPassword123",
    "email":    "optional@example.com",
    "clientVersion": "0.1.2"
  }
}
```

| Field      | Type   | Required | Constraints                              |
|------------|--------|----------|------------------------------------------|
| `login`    | string | Yes      | 3–20 chars, `[A-Za-z0-9_]` only         |
| `password` | string | Yes      | 8–100 chars                              |
| `email`    | string | No       | Must contain `@` if provided            |

### Success Response
```json
{
  "header": {
    "eventType": "registerAccount",
    "status":    "success",
    "message":   "Registration successful",
    "clientId":  42,
    "hash":      "550e8400-e29b-41d4-a716-446655440000",
    "login":     "MyLogin"
  },
  "body": {}
}
```

### Error Codes

| `message`               | Meaning                                    |
|-------------------------|--------------------------------------------|
| `ERR_LOGIN_INVALID`     | Login format check failed                  |
| `ERR_LOGIN_TAKEN`       | Login already registered (case-insensitive)|
| `ERR_PASSWORD_TOO_SHORT`| Password shorter than 8 characters        |
| `ERR_PASSWORD_TOO_LONG` | Password longer than 100 characters       |
| `ERR_EMAIL_INVALID`     | Email provided but has no `@`             |
| `ERR_REGISTER_FAILED`   | Internal/DB error                          |
| `ERR_DB`                | Postgres unreachable (fast-fail, retry later) |

---

## 2. Authenticate (Login)

**Request eventType:** `authentificationClient`  
**Auth required:** No

### Request
```json
{
  "header": { "eventType": "authentificationClient" },
  "body": {
    "login":    "MyLogin",
    "password": "MyPassword123",
    "clientVersion": "0.1.2"
  }
}
```

### Success Response
```json
{
  "header": {
    "eventType": "authentificationClient",
    "status":    "success",
    "message":   "Authentication success for user!",
    "clientId":  42,
    "hash":      "550e8400-e29b-41d4-a716-446655440000",
    "login":     "MyLogin"
  },
  "body": {}
}
```

### Error Response
```json
{
  "header": {
    "eventType": "authentificationClient",
    "status":    "error",
    "message":   "Authentication failed for user!",
    "clientId":  0,
    "hash":      ""
  },
  "body": {
    "errorCode": "Authentication failed for user!"
  }
}
```

Machine-readable auth failures (`body.errorCode` mirrors `message`):

| `message` / `errorCode`       | Meaning                                              |
|-------------------------------|------------------------------------------------------|
| `ERR_ACCOUNT_LOCKED`          | Too many failed logins, account temporarily locked   |
| `ERR_ACCOUNT_BANNED`          | Manual GM ban (`user_bans`); body adds `banReason` + `banExpiresAt` ("" = permanent). Пин: `test_ban.py` |
| `ERR_DB`                      | Postgres unreachable — fail fast, retry (never hang) |
| `ERR_INTERNAL`                | Session vanished between store and read (rare race)  |
| `Authentication failed for user!` | Wrong login/password (deliberately vague)        |

---

## 3. Get Character Creation Options

**Request eventType:** `getCharacterCreationOptions`  
**Auth required:** Yes (`clientId` + `hash`)

Returns available classes, races and genders for the character creation screen. Call this **once** after login/registration, before showing the creation UI.

### Request
```json
{
  "header": {
    "eventType": "getCharacterCreationOptions",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {}
}
```

### Success Response
```json
{
  "header": {
    "eventType": "getCharacterCreationOptions",
    "status":    "success",
    "message":   "Options retrieved successfully",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "classes": [
      { "id": 1, "name": "Mage",    "slug": "mage",    "description": "..." },
      { "id": 2, "name": "Warrior", "slug": "warrior", "description": "..." }
    ],
    "races": [
      { "id": 1, "name": "Human", "slug": "human" }
    ],
    "genders": [
      { "id": 0, "slug": "male",   "label": "Male"   },
      { "id": 1, "slug": "female", "label": "Female" }
    ]
  }
}
```

Use the `slug` field values in the `createCharacter` request.

---

## 4. Get Characters List

**Request eventType:** `getCharactersList`  
**Auth required:** Yes

### Request
```json
{
  "header": {
    "eventType": "getCharactersList",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {}
}
```

### Success Response
```json
{
  "header": {
    "eventType": "getCharactersList",
    "status":    "success",
    "message":   "Characters list retrieved successfully!",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "charactersList": [
      {
        "characterId":    7,
        "characterName":  "Gandalf",
        "classSlug":      "mage",
        "raceSlug":       "human",
        "genderSlug":     "male",
        "characterLevel": 5,
        "equipment": [
          { "slotId": 1, "itemSlug": "wooden_staff" },
          { "slotId": 5, "itemSlug": "leather_boots" }
        ]
      }
    ]
  }
}
```

Empty `charactersList` array `[]` means the account has no characters yet.  
Empty `equipment` array `[]` means the character has no equipped items (new character).

---

## 5. Create Character

**Request eventType:** `createCharacter`  
**Auth required:** Yes  
**Limit:** Max 4 characters per account

### Request
```json
{
  "header": {
    "eventType": "createCharacter",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "characterName":   "Gandalf",
    "characterClass":  "Mage",
    "characterRace":   "Human",
    "characterGender": "male"
  }
}
```

| Field             | Type   | Required | Source                          |
|-------------------|--------|----------|---------------------------------|
| `characterName`   | string | Yes      | 2–20 chars, letters and spaces  |
| `characterClass`  | string | Yes      | `slug` from `getCharacterCreationOptions` → `classes` |
| `characterRace`   | string | Yes      | `slug` from `getCharacterCreationOptions` → `races`   |
| `characterGender` | string | Yes      | `slug` from `getCharacterCreationOptions` → `genders` |

The server automatically:
- Grants default skills for the chosen class
- Adds starter items (weapon + 50 gold) to the inventory
- Sets starting position (zone 1 — village)
- Initialises HP/MP

### Success Response
```json
{
  "header": {
    "eventType": "createCharacter",
    "status":    "success",
    "message":   "Character created successfully",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "characterId": 7
  }
}
```

### Error Codes

| `message`                 | Meaning                                     |
|---------------------------|---------------------------------------------|
| `ERR_CHAR_NAME_TAKEN`     | Name already used (case-insensitive)        |
| `ERR_CHAR_NAME_INVALID`   | Name format check failed                   |
| `ERR_CHAR_SLOT_FULL`      | Account has reached the 4-character limit  |
| `ERR_CHAR_MISSING_FIELD`  | One or more required fields are empty      |
| `ERR_CHAR_CREATE_FAILED`  | Internal/DB error                           |
| `Unauthorized`            | Invalid or missing `clientId` / `hash`     |

---

## 6. Delete Character

**Request eventType:** `deleteCharacter`  
**Auth required:** Yes  
**Note:** Soft-delete — character data is retained in the DB with `deleted_at` timestamp. The operation is **irreversible** from the client's perspective.

### Request
```json
{
  "header": {
    "eventType": "deleteCharacter",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "characterId": 7
  }
}
```

### Success Response
```json
{
  "header": {
    "eventType": "deleteCharacter",
    "status":    "success",
    "message":   "Character deleted successfully",
    "clientId":  42,
    "hash":      "550e8400-..."
  },
  "body": {
    "characterId": 7
  }
}
```

### Error Codes

| `message`                    | Meaning                                           |
|------------------------------|---------------------------------------------------|
| `ERR_CHARACTER_NOT_FOUND`    | Character doesn't exist or belongs to another account |
| `ERR_INVALID_CHARACTER_ID`   | `characterId` is missing or zero                |
| `Unauthorized`               | Invalid or missing `clientId` / `hash`           |

---

## 7. Ping

**Request eventType:** `pingClient`  
**Auth required:** No

### Request
```json
{
  "header": { "eventType": "pingClient" }
}
```

### Response
```json
{
  "header": {
    "eventType":        "pingClient",
    "status":           "success",
    "message":          "Pong!",
    "serverRecvMs":     1713567890123,
    "serverSendMs":     1713567890125,
    "clientSendMsEcho": 1713567890100
  },
  "body": {}
}
```

---

## Typical Client Flow

```
1. [Optional] registerAccount  →  receive clientId + hash
            OR
   authentificationClient      →  receive clientId + hash

2. getCharacterCreationOptions  →  populate class/race/gender dropdowns

3a. [New account] createCharacter  →  receive characterId
3b. [Existing]   getCharactersList →  show character list
                 createCharacter   →  (if creating new)

4. deleteCharacter  →  (optional, on character deletion screen)

5. Connect to Game Server with clientId + hash + characterId
   (see Game Server / Chunk Server protocol documentation)
```

---

## Error Handling Guidelines

- All error responses have `"status": "error"` in the header.
- The `message` field contains a machine-readable error code (`ERR_*`) or a human-readable string.
- **Match on `body.errorCode`** (always present on errors), not on `message` text.
- Map `ERR_*` codes to localized UI strings on the client side; never display raw codes to the player.
- On `Unauthorized` response: clear stored `clientId`/`hash` and redirect to the login screen.
- On `ERR_DB`: back off and retry — the server answers fast (5s pool timeout) and recovers transparently when Postgres returns.

---

## Ops: sessions, cleanup, DB outages

- Session expiry is DB truth (`cleanup_expired_sessions`); cadence via `session_cleanup_ms` (config.json + `SESSION_CLEANUP_MS` env, default 60s). In-memory sweep drops entries idle > 2× cadence.
- Pool (`DatabasePool`, 5 conns): health-checked `SELECT 1` on acquire, transparent reopen on drops, reconnect/timeout counters in logs.
- DB-down contract: auth during a Postgres outage returns `ERR_DB` (never hangs); after `docker start` the pools reconnect with no server restart. Pinned by standalone `test_dbdown.py` (nightly only — it stops the shared dev DB, never add to batches).
