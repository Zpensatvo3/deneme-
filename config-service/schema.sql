CREATE TABLE IF NOT EXISTS public_configs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_hash TEXT NOT NULL,
    author_name TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    payload TEXT NOT NULL,
    module_count INTEGER NOT NULL,
    downloads INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS public_configs_owner ON public_configs(owner_hash);
CREATE INDEX IF NOT EXISTS public_configs_updated ON public_configs(updated_at DESC, id DESC);

CREATE TABLE IF NOT EXISTS request_limits (
    client_hash TEXT NOT NULL,
    operation TEXT NOT NULL,
    window_start INTEGER NOT NULL,
    count INTEGER NOT NULL,
    PRIMARY KEY (client_hash, operation)
);
