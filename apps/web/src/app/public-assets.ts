/** Also usable by Node/tsx tests, without Vite environment transforms. */
export function publicAsset(path: string, base = '/'): string {
    if (/^https:\/\//.test(path)) return path;
    if (!base.startsWith('/') || !base.endsWith('/') || path.startsWith('/') || path.includes('..')) throw new Error('Invalid public asset path');
    return `${base}${path}`;
}

export async function verifiedRuntime(response: Response, expected: { bundle_sha256: string; bundle_bytes: number }): Promise<Uint8Array> {
    if (!response.ok) throw new Error(`Runtime request failed (${response.status})`);
    if (!response.headers.get('content-type')?.includes('application/json')) throw new Error('Runtime response was not JSON');
    const bytes = new Uint8Array(await response.arrayBuffer());
    const digest = await crypto.subtle.digest('SHA-256', bytes);
    const actual = Array.from(new Uint8Array(digest), b => b.toString(16).padStart(2, '0')).join('');
    if (bytes.length !== expected.bundle_bytes || actual !== expected.bundle_sha256) throw new Error('Runtime integrity check failed');
    return bytes;
}
