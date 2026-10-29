# Public config service design

Status: the Worker and D1 database have been deployed and basic live API
operations tested. The native client transport has been implemented but still
requires a Windows build and in-game verification.

## Goal

Any user can browse and load public configs, and can publish configs without
logging in or entering a key. A publisher can update or delete their own
entries. The service is independent of the removed launcher and license system.

## Architecture

The native client sends HTTPS requests to a dedicated Cloudflare Worker. The
Worker validates requests and accesses a new D1 database through its binding.
The existing config UI and module serialization stay in the client; requests
run off the render thread and downloaded settings are applied on the menu
thread as before. Network failure must leave existing local settings intact.

## Ownership without manual keys

On first use the client creates a cryptographically random 256-bit installation
secret using Windows CNG, stores it in the user's local application data with
Windows DPAPI protection, and reuses it automatically. Send it only over HTTPS;
never put it in URLs, UI, logs, public config payloads or the repository. The
server stores only its SHA-256 hash and compares that hash for update/delete.
Listing may use the secret to mark the user's own rows; loading needs no secret.

This is a device-local bearer credential, not a user account. Anyone who steals
it can modify that installation's configs. Reinstalling, changing Windows user,
or losing the stored secret can make existing configs impossible to edit; no
account-based recovery is promised. A separate recovery design is needed if
cross-device management becomes a requirement. Minecraft display names are
unverified labels and must never grant ownership.

## API and data

Provide list, load, publish/update and delete operations. List currently returns
up to 80 entries; pagination remains to be implemented. Load returns a JSON
payload validated when published and again by the client when applied. Publish accepts name,
description, display name, module count and payload. Updates and
deletes use a server-assigned ID plus the installation secret. Return explicit
400, 403, 404, 429 and service error responses; never silently apply an invalid
config. Validate JSON structure, sizes, count and string limits on both sides;
use parameterized SQL. Keep the existing stable module IDs and config field
compatibility. No license or HWID data is needed.

## Abuse and operations

Before deployment, implement bounded request reads, rate limits for listing,
loading and especially publishing/deletion, publication quotas, and sensible
response limits. Treat rate limiting as abuse reduction rather than proof of
identity. Store owner hashes rather than tokens; redact request bodies and
credentials from logs. Provide a way for the operator to remove abusive entries
without exposing an unprotected public admin endpoint. Monitor errors and D1
growth, and establish backup and deletion policies.

## Cutover and verification

1. Expand Worker tests for concurrency and operational limits; current tests
   cover basic ownership, malformed input and per-IP request limits.
2. Verify Windows client transport and secure secret persistence on Windows,
   including invalid responses, shutdown and update/delete ownership.
3. The new Worker and D1 database are deployed; a test config was published,
   loaded, tested against another owner, and deleted.
4. Decide whether old public configs need migration before removing old live
   Cloudflare resources. Ownership cannot automatically transfer from old
   license/HWID identities to a new installation secret. Do not delete the old
   service or database before their exact resources and data are checked.
5. Build on Windows and test the full flow in the target Minecraft version.

The previous `onestep-auth` Worker, `onestep-licenses` D1 database and
`ONESTEP_RELEASES` KV namespace were deleted on 2026-10-29. The new public
config Worker uses a separate D1 database.
