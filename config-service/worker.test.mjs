import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
import worker from './worker.js';

function setup() {
    const db = new DatabaseSync(':memory:');
    db.exec(readFileSync(new URL('./schema.sql', import.meta.url), 'utf8'));
    return { DB: { prepare(sql) {
        return { bind(...params) {
            return {
                async first() { return db.prepare(sql).get(...params) || null; },
                async all() { return { results: db.prepare(sql).all(...params) }; },
                async run() { const result = db.prepare(sql).run(...params); return { meta: { changes: Number(result.changes), last_row_id: Number(result.lastInsertRowid) } }; }
            };
        } };
    } } };
}
const token = 'a'.repeat(64), other = 'b'.repeat(64);
const input = { name: 'Example', description: '', author_name: 'Player_1', payload: '{"modules":[]}', module_count: 0 };
async function request(env, operation, body, ip = '127.0.0.1') {
    return worker.fetch(new Request(`https://example.test/api/v1/configs/${operation}`, {
        method: 'POST', headers: { 'CF-Connecting-IP': ip }, body: JSON.stringify(body)
    }), env);
}

test('public config lifecycle and ownership isolation', async () => {
    const env = setup();
    const created = await (await request(env, 'publish', { ...input, owner_token: token })).json();
    assert.equal(created.success, true);
    assert.equal((await request(env, 'publish', { ...input, id: created.id, owner_token: other })).status, 403);
    assert.equal((await request(env, 'delete', { id: created.id, owner_token: other })).status, 403);
    const listing = await (await request(env, 'list', { owner_token: token })).json();
    assert.equal(listing.configs[0].mine, 1);
    assert.equal((await (await request(env, 'load', { id: created.id })).json()).config.payload, input.payload);
    assert.equal((await request(env, 'delete', { id: created.id, owner_token: token })).status, 200);
    assert.equal((await request(env, 'load', { id: created.id })).status, 404);
});

test('rejects malformed requests and limits repeated writes', async () => {
    const env = setup();
    assert.equal((await request(env, 'publish', { ...input, owner_token: 'bad' })).status, 400);
    assert.equal((await request(env, 'publish', { ...input, owner_token: token, payload: 'invalid' })).status, 400);
    for (let i = 0; i < 20; ++i) assert.equal((await request(env, 'delete', { id: 1, owner_token: token }, '192.0.2.1')).status, 403);
    assert.equal((await request(env, 'delete', { id: 1, owner_token: token }, '192.0.2.1')).status, 429);
});
