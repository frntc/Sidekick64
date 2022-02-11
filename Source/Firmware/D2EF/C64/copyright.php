<?php

$text = '  ALEX\'S DISK2EASYFLASH - "LOAD" V0.92  ';

for ($i = 0; $i < strlen($text); $i++) {
  if (ord($text[$i]) >= 0x41 && ord($text[$i]) <= 0x5A) {
    $text[$i] = chr(ord($text[$i]) - 0x40);
  }
}

file_put_contents("copyright.bin", $text);
