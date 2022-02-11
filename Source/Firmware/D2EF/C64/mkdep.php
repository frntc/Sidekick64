<?php

$target = $argv[1];
$exclude = array_slice($argv, 2);

echo substr($argv[1], 0, -3).'prg: '.implode(' ', mkdep($argv[1]))."\n";

function mkdep($src) {
  global $exclude;

  if (!is_file($src))
    return array();

  $file = file_get_contents($src);

  if (!preg_match_all('!.*\.import [a-z0-9]+ "(.*?)"!', $file, $M, PREG_SET_ORDER))
    return array();

  $deps = array();
  foreach ($M as $ln) {
    if (strpos($ln[0], '//') === false && array_search($ln[1], $exclude) === false)
      $deps = array_merge($deps, array($ln[1]), mkdep($ln[1]));
  }

  return $deps;
}
