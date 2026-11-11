<?php
declare(strict_types=1);

/* Public GitHub metadata only. No credentials or visitor input are used. */
function bitskim_release_escape(string $text): string {
    return htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function bitskim_select_releases(array $releases): array {
    $patterns = [
        'mac-dmg' => 'macOS-arm64\.dmg', 'mac-zip' => 'macOS-arm64\.zip',
        'windows-setup' => 'Windows10-x64-Setup\.exe', 'windows-zip' => 'Windows10-x64\.zip',
        'amiga-zip' => 'AmigaOS\.zip', 'dos-zip' => 'DOS-mTCP\.zip',
        'riscos-arm2' => 'RISCOS3-ARM2\.zip', 'riscos-strongarm' => 'RISCOS3-StrongARM\.zip',
        'atari-sting' => 'Atari-TOS-STinG-68000\.zip', 'atari-mintnet' => 'Atari-MiNTNet-68000\.zip',
        'atari-mintnet060' => 'Atari-MiNTNet-68060\.zip', 'windows-classic' => 'Windows-Classic-x86\.zip',
    ];
    $releases = array_values(array_filter($releases, static fn($r) => is_array($r) && empty($r['draft']) && empty($r['prerelease']) && is_string($r['tag_name'] ?? null) && preg_match('/^[A-Za-z0-9][A-Za-z0-9._+\/-]{0,99}$/', $r['tag_name']) && is_string($r['published_at'] ?? null)));
    usort($releases, static fn($a, $b) => strcmp($b['published_at'], $a['published_at']));
    $catalog = ['clients' => [], 'release_version' => null, 'source' => null, 'release_url' => 'https://github.com/retrocs-org/bitskim/releases'];
    foreach ($releases as $release) {
        $tag = $release['tag_name'];
        $releaseUrl = 'https://github.com/retrocs-org/bitskim/releases/tag/' . rawurlencode($tag);
        if ($catalog['source'] === null) {
            $catalog['source'] = 'https://github.com/retrocs-org/bitskim/archive/refs/tags/' . rawurlencode($tag) . '.zip';
            $catalog['release_version'] = preg_replace('/^v(?=[0-9])/', '', $tag);
            $catalog['release_url'] = $releaseUrl;
        }
        $assets = [];
        foreach ((is_array($release['assets'] ?? null) ? $release['assets'] : []) as $asset) {
            if (!is_array($asset) || !is_string($asset['name'] ?? null) || !is_string($asset['browser_download_url'] ?? null)) continue;
            $url = $asset['browser_download_url']; $parts = parse_url($url);
            if (!is_array($parts) || ($parts['scheme'] ?? '') !== 'https' || ($parts['host'] ?? '') !== 'github.com' || isset($parts['user']) || isset($parts['pass']) || isset($parts['query']) || isset($parts['fragment']) || isset($parts['port'])) continue;
            if (!str_starts_with($url, 'https://github.com/retrocs-org/bitskim/releases/download/') || rawurldecode(basename($parts['path'] ?? '')) !== $asset['name']) continue;
            if (($asset['state'] ?? 'uploaded') !== 'uploaded') continue;
            $assets[$asset['name']] = $url;
        }
        foreach ($assets as $name => $url) {
            $id = null; $version = null;
            if (preg_match('/^BitSkim-([0-9][A-Za-z0-9.+_-]{0,63})\.lha$/i', $name, $match)) { $id = 'amiga-lha'; $version = $match[1]; }
            foreach ($patterns as $key => $suffix) {
                if (preg_match('/^BitSkim-([0-9][A-Za-z0-9.+_-]{0,63}?)-' . $suffix . '$/i', $name, $match)) { $id = $key; $version = $match[1]; break; }
            }
            if ($id !== null && !isset($catalog['clients'][$id])) {
                $catalog['clients'][$id] = ['url' => $url, 'version' => $version, 'checksum' => $assets[$name . '.sha256'] ?? null, 'release_url' => $releaseUrl, 'bundled' => false];
            }
        }
    }
    return $catalog;
}

function bitskim_fetch_release_data(): ?array {
    $url = 'https://api.github.com/repos/retrocs-org/bitskim/releases?per_page=100';
    if (function_exists('curl_init')) {
        $curl = curl_init($url); $body = '';
        curl_setopt_array($curl, [CURLOPT_USERAGENT => 'BitSkim-Website', CURLOPT_HTTPHEADER => ['Accept: application/vnd.github+json'], CURLOPT_CONNECTTIMEOUT => 3, CURLOPT_TIMEOUT => 3, CURLOPT_FOLLOWLOCATION => false, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2, CURLOPT_WRITEFUNCTION => static function ($handle, $chunk) use (&$body) { if (strlen($body) + strlen($chunk) > 2097152) return 0; $body .= $chunk; return strlen($chunk); }]);
        $ok = curl_exec($curl); $status = curl_getinfo($curl, CURLINFO_RESPONSE_CODE); curl_close($curl);
        if ($ok === false || $status !== 200) return null;
        $data = json_decode($body, true);
        return is_array($data) && (empty($data) || isset($data[0])) ? $data : null;
    }
    if (!in_array('https', stream_get_wrappers(), true) || !ini_get('allow_url_fopen')) return null;
    $context = stream_context_create([
        'http' => ['method' => 'GET', 'header' => "User-Agent: BitSkim-Website\r\nAccept: application/vnd.github+json\r\n", 'timeout' => 3, 'follow_location' => 0, 'ignore_errors' => false],
        'ssl' => ['verify_peer' => true, 'verify_peer_name' => true],
    ]);
    $body = @file_get_contents($url, false, $context, 0, 2097152);
    if ($body === false) return null;
    $data = json_decode($body, true);
    return is_array($data) && (empty($data) || isset($data[0])) ? $data : null;
}

function bitskim_load_releases(?callable $fetcher = null, ?string $cachePath = null, ?int $now = null): array {
    if (getenv('BITSKIM_RELEASES_OFFLINE') === '1' && $fetcher === null) return bitskim_select_releases([]);
    $now = $now ?? time();
    $cachePath = $cachePath ?? sys_get_temp_dir() . '/bitskim-public-releases-' . substr(hash('sha256', __DIR__), 0, 16) . '.json';
    $cached = is_file($cachePath) ? json_decode((string)@file_get_contents($cachePath), true) : null;
    $valid = is_array($cached) && is_array($cached['releases'] ?? null) && is_int($cached['checked'] ?? null);
    if ($valid && $now - $cached['checked'] < 900) return bitskim_select_releases($cached['releases']);
    $lock = @fopen($cachePath . '.lock', 'c');
    if ($lock === false || !flock($lock, LOCK_EX | LOCK_NB)) {
        if (is_resource($lock)) fclose($lock);
        return bitskim_select_releases($valid ? $cached['releases'] : []);
    }
    try {
        $data = ($fetcher ?? 'bitskim_fetch_release_data')();
        if (!is_array($data)) $data = $valid ? $cached['releases'] : [];
        $result = ['checked' => $now, 'releases' => $data];
        $temporary = $cachePath . '.' . getmypid() . '.tmp';
        if (@file_put_contents($temporary, json_encode($result)) !== false) {
            @chmod($temporary, 0600); @rename($temporary, $cachePath);
        }
        return bitskim_select_releases($data);
    } finally {
        flock($lock, LOCK_UN); fclose($lock);
    }
}

function bitskim_client_download(string $id, string $fallback): array {
    global $releaseCatalog;
    return $releaseCatalog['clients'][$id] ?? ['url' => 'https://github.com/retrocs-org/bitskim/releases/download/0.1/' . rawurlencode(basename($fallback)), 'version' => '0.1', 'checksum' => null, 'release_url' => 'release-notes.php', 'bundled' => true];
}

function bitskim_source_url(): string {
    global $releaseCatalog;
    return $releaseCatalog['source'] ?? 'https://github.com/retrocs-org/bitskim/archive/refs/tags/0.1.zip';
}

function bitskim_client_card(string $name, string $status, string $description, array $links): void {
    $downloads = [];
    foreach ($links as [$id, $label, $fallback]) $downloads[] = ['label' => $label] + bitskim_client_download($id, $fallback);
    $versions = array_values(array_unique(array_column($downloads, 'version')));
    $bundled = array_filter($downloads, static fn($d) => $d['bundled']);
    echo '<article class="client-card"><div class="client-heading"><h3>' . bitskim_release_escape($name) . '</h3><span class="pill' . ($status === 'Available' ? '' : ' muted') . '">' . bitskim_release_escape($status) . '</span></div>';
    echo '<span class="client-version">' . (count($versions) === 1 ? 'Version ' : 'Versions ') . bitskim_release_escape(implode(' / ', $versions)) . (count($bundled) > 0 ? ' · fallback' : '') . '</span><p>' . bitskim_release_escape($description) . '</p><div class="client-links">';
    foreach ($downloads as $d) {
        echo '<span><a href="' . bitskim_release_escape($d['url']) . '" download>' . bitskim_release_escape($d['label'] . (count($versions) > 1 ? ' · ' . $d['version'] : '')) . ' ↓</a>';
        if ($d['checksum'] !== null) echo ' <a class="checksum-link" href="' . bitskim_release_escape($d['checksum']) . '">SHA-256</a>';
        echo '</span>';
    }
    echo '</div></article>';
}
