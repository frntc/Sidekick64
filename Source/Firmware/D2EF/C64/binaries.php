<?php

$fc = fopen("binaries.c", "w");
fwrite($fc, "#include <stdint.h>\n");

$fh = fopen("binaries.h", "w");
fwrite($fh, "#ifndef BINARIES_H
#define BINARIES_H

#include <stdint.h>
");

foreach (array(
           'kapi_hi.prg' => 2,
           'kapi_nm.prg' => 2,
           'kapi_lo.prg' => 2,
           'launcher_hi.bin' => 0,
           'startup.bin' => 0,
           'sprites.bin' => 0,
         ) AS $file => $offset) {
  $bin = substr(file_get_contents($file), $offset);
  fwrite($fc, 'uint8_t '.substr($file, 0, -4).'[] = {');
  for ($i = 0; $i < strlen($bin); $i++) {
    fwrite($fc, ord($bin[$i]).', ');
  }
  fwrite($fc, "};\n");

  fwrite($fh, 'extern uint8_t '.substr($file, 0, -4)."[];\n");
  fwrite($fh, '#define '.substr($file, 0, -4).'_size '.strlen($bin)."\n");
}

fwrite($fh, "#endif\n");
