/*
  _________.__    .___      __   .__        __      ________  ________   _____     _______              __________    _____      _____    .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __  \_____  \/  _____/  /  |  |    \      \   ____  ____\______   \  /  _  \    /     \   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   /  ____/   __  \  /   |  |_   /   |   \_/ __ \/  _ \|       _/ /  /_\  \  /  \ /  \  |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <   /       \  |__\  \/    ^   /  /    |    \  ___(  <_> )    |   \/    |    \/    Y    \ |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \  \_______ \_____  /\____   |   \____|__  /\___  >____/|____|_  /\____|__  /\____|__  / |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/          \/     \/      |__|           \/     \/             \/         \/         \/          \/    \/           \/     \/     \/ 

 launchNeoRAM.cpp

 RasPiC64 - A framework for interfacing the C64 (and the C16/+4) and a Raspberry Pi 3B/3B+
          - simple launch code for running Alpharay and Pet's Rescue NeoRAM versions on the 264-series
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

 Logo created with http://patorjk.com/software/taag/
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <conio.h>


int main (void)
{
    __asm__ ("lda #$0");
    
    __asm__ ("sta $ff15");	// background color
    __asm__ ("sta $ff19");	// border color

    __asm__ ("sta $fde8");	
    __asm__ ("sta $fde9");	
    __asm__ ("sta $fdea");	

    __asm__ ("jmp $fe00");	// go!

    return EXIT_SUCCESS;
}
