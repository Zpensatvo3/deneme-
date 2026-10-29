const MAX_BODY = 512 * 1024;
const json = (body, status = 200) => Response.json(body, { status, headers: { 'Cache-Control': 'no-store' } });

async function readBody(request) {
    const reader = request.body?.getReader();
    if (!reader) throw new Error('Missing body');
    const chunks = [];
    let size = 0;
    while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        size += value.length;
        if (size > MAX_BODY) { await reader.cancel(); throw new Error('Body too large'); }
        chunks.push(value);
    }
    const bytes = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return JSON.parse(new TextDecoder().decode(bytes));
}

async function ownerHash(token) {
    if (typeof token !== 'string' || !/^[0-9a-f]{64}$/.test(token)) throw new Error('Invalid owner token');
    const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(token));
    return Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
}

async function rateLimit(request, env, path) {
    const ip = request.headers.get('CF-Connecting-IP');
    if (!ip) return false;
    const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(ip));
    const client = Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
    const operation = path.endsWith('/publish') || path.endsWith('/delete') ? 'write' : 'read';
    const limit = operation === 'write' ? 20 : 120;
    const window = Math.floor(Date.now() / 60000);
    // Periodically bound stale client entries without keeping request state globally.
    if (client.startsWith('00'))
        await env.DB.prepare('DELETE FROM request_limits WHERE window_start < ?').bind(window - 1440).run();
    const row = await env.DB.prepare(
        'INSERT INTO request_limits (client_hash, operation, window_start, count) VALUES (?, ?, ?, 1) ' +
        'ON CONFLICT(client_hash, operation) DO UPDATE SET count = CASE WHEN window_start = excluded.window_start THEN count + 1 ELSE 1 END, window_start = excluded.window_start RETURNING count'
    ).bind(client, operation, window).first();
    return row && row.count <= limit;
}

function configInput(body) {
    const { name, description, author_name, payload, module_count } = body;
    if (typeof name !== 'string' || name.trim().length < 2 || name.length > 48 ||
        typeof description !== 'string' || description.length > 180 || /[\u0000-\u001f]/.test(name + description) ||
        typeof author_name !== 'string' || !/^[A-Za-z0-9_]{3,16}$/.test(author_name) ||
        typeof payload !== 'string' || payload.length < 8 || new TextEncoder().encode(payload).length > 96 * 1024 ||
        !Number.isSafeInteger(module_count) || module_count < 0 || module_count > 128) throw new Error('Invalid config');
    const parsed = JSON.parse(payload);
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed) ||
        !Array.isArray(parsed.modules) || parsed.modules.length > 128) throw new Error('Invalid payload');
    return [author_name, name.trim(), description, payload, module_count];
}

export default {
    async fetch(request, env) {
        if (request.method !== 'POST') return json({ success: false, message: 'Method not allowed.' }, 405);
        const path = new URL(request.url).pathname;
        if (!['/api/v1/configs/list', '/api/v1/configs/load', '/api/v1/configs/publish', '/api/v1/configs/delete'].includes(path))
            return json({ success: false, message: 'Not found.' }, 404);
        let body, owner = null;
        try {
            body = await readBody(request);
            if (!body || typeof body !== 'object' || Array.isArray(body)) throw new Error('Invalid body');
            if (body.owner_token !== undefined) owner = await ownerHash(body.owner_token);
            if ((path.endsWith('/publish') || path.endsWith('/delete')) && !owner) throw new Error('Owner required');
        }
        catch { return json({ success: false, message: 'Invalid request.' }, 400); }
        try {
            if (!await rateLimit(request, env, path)) return json({ success: false, message: 'Too many requests.' }, 429);
            if (path.endsWith('/list')) {
                const rows = await env.DB.prepare('SELECT id, author_name, name, description, module_count, downloads, created_at, updated_at, CASE WHEN owner_hash = ? THEN 1 ELSE 0 END AS mine FROM public_configs ORDER BY updated_at DESC, id DESC LIMIT 80').bind(owner).all();
                return json({ success: true, configs: rows.results || [] });
            }
            const id = Number(body.id || 0);
            if (!Number.isSafeInteger(id) || id < 0 || (!path.endsWith('/publish') && id === 0))
                return json({ success: false, message: 'Invalid config id.' }, 400);
            if (path.endsWith('/load')) {
                const config = await env.DB.prepare('SELECT id, author_name, name, description, payload, module_count, downloads, created_at, updated_at FROM public_configs WHERE id = ?').bind(id).first();
                if (!config) return json({ success: false, message: 'Config not found.' }, 404);
                await env.DB.prepare('UPDATE public_configs SET downloads = downloads + 1 WHERE id = ?').bind(id).run();
                return json({ success: true, config: { ...config, downloads: Number(config.downloads) + 1 } });
            }
            if (path.endsWith('/delete')) {
                const result = await env.DB.prepare('DELETE FROM public_configs WHERE id = ? AND owner_hash = ?').bind(id, owner).run();
                return result.meta?.changes === 1 ? json({ success: true, message: 'Config deleted.' }) : json({ success: false, message: 'Config not found or not owned.' }, 403);
            }
            let values;
            try { values = configInput(body); } catch { return json({ success: false, message: 'Invalid config.' }, 400); }
            if (id) {
                const result = await env.DB.prepare('UPDATE public_configs SET author_name = ?, name = ?, description = ?, payload = ?, module_count = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ? AND owner_hash = ?').bind(...values, id, owner).run();
                return result.meta?.changes === 1 ? json({ success: true, id, message: 'Config updated.' }) : json({ success: false, message: 'Config not found or not owned.' }, 403);
            }
            const result = await env.DB.prepare('INSERT INTO public_configs (owner_hash, author_name, name, description, payload, module_count) SELECT ?, ?, ?, ?, ?, ? WHERE (SELECT COUNT(*) FROM public_configs WHERE owner_hash = ?) < 20').bind(owner, ...values, owner).run();
            if (result.meta?.changes !== 1) return json({ success: false, message: 'Config limit reached.' }, 429);
            return json({ success: true, id: result.meta.last_row_id, message: 'Config published.' });
        } catch (error) {
            console.error('config_service_error', error?.message);
            return json({ success: false, message: 'Config service unavailable.' }, 500);
        }
    }
};
