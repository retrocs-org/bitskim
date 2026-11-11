<?php
if (!isset($site)) {
    http_response_code(404);
    exit;
}
?>
<footer class="wrap"><a class="brand" href="index.php">BitSkim<span class="version"><?= site_escape($site['version']) ?></span></a><p><a href="<?= site_escape($site['url']) ?>">bitskim.retrocs.org</a></p><a href="<?= site_escape($site['repository']) ?>">GitHub ↗</a><a href="<?= $isHome ? '#support' : 'index.php#support' ?>">Support</a><a href="release-notes.php">Release notes ↗</a></footer>
</body></html>
