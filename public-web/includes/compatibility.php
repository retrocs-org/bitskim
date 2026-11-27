<?php
declare(strict_types=1);
function bitskim_compatibility_table(): void {
    $catalog = json_decode((string)file_get_contents(__DIR__ . '/compatibility.json'), true, 512, JSON_THROW_ON_ERROR);
    $escape = static fn(string $text): string => htmlspecialchars($text, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
    echo '<p>Checks completed for version ' . $escape($catalog['version']) . '. Download availability is listed above; a passing build does not certify untested hardware.</p>';
    echo '<div class="compatibility-scroll" tabindex="0" role="region" aria-label="Client compatibility"><table class="compatibility-table"><thead><tr><th scope="col">Client / target</th><th scope="col">Checks completed</th><th scope="col">Still unverified</th></tr></thead><tbody>';
    foreach ($catalog['clients'] as $client) {
        echo '<tr><th scope="row">' . $escape($client['name']) . '<br><small>' . $escape($client['target']) . '</small></th><td>' . $escape($client['verified']) . '</td><td>' . $escape($client['pending']) . '</td></tr>';
    }
    echo '</tbody></table></div><p>' . $escape($catalog['scope']) . '</p>';
}
