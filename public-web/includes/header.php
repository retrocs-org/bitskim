<?php
if (!isset($pageTitle, $pageDescription, $canonicalPath, $isHome)) {
    http_response_code(404);
    exit;
}
$site = require __DIR__ . '/config.php';
require_once __DIR__ . '/releases.php';
$releaseCatalog = bitskim_load_releases();
if ($releaseCatalog['release_version'] !== null) $site['version'] = $releaseCatalog['release_version'];
function site_escape(string $value): string {
    return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}
?>
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="<?= site_escape($pageDescription) ?>">
<meta name="theme-color" content="#171d23"><title><?= site_escape($pageTitle) ?></title>
<link rel="icon" href="assets/favicon.png" type="image/png"><link rel="stylesheet" href="style.css?v=<?= substr(hash_file('sha256', __DIR__ . '/../style.css'), 0, 12) ?>">
<link rel="canonical" href="<?= site_escape($site['url'] . $canonicalPath) ?>">
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<header class="wrap"><a class="brand" href="index.php"><img src="assets/bitskim.png" width="38" height="38" alt="">BitSkim<span class="version"><?= site_escape($site['version']) ?></span></a>
<?php if ($isHome): ?>
<nav aria-label="Main navigation"><a href="#screenshots">Screenshots</a><a href="#how">How it works</a><a href="#get">Clients</a><a href="#build">Build from source</a><a class="support-nav" href="<?= $isHome ? '#support' : 'index.php#support' ?>">Support</a><a class="nav-cta" href="#get">Get BitSkim <span aria-hidden="true">↗</span></a></nav>
<?php else: ?>
<a href="index.php">← Back to BitSkim</a>
<?php endif; ?>
</header>
