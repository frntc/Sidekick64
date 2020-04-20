;   _________.__    .___      __   .__        __        _____                            ___ ________  ________   _____   ___    
;   /   _____/|__| __| _/____ |  | _|__| ____ |  | __   /     \   ____   ____  __ __     /  / \_____  \/  _____/  /  |  |  \  \   
;  \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /  /  \ /  \_/ __ \ /    \|  |  \   /  /   /  ____/   __  \  /   |  |_  \  \  
;  /        \|  / /_/ \  ___/|    <|  \  \___|    <  /    Y    \  ___/|   |  \  |  /  (  (   /       \  |__\  \/    ^   /   )  ) 
; /_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \ \____|__  /\___  >___|  /____/    \  \  \_______ \_____  /\____   |   /  /  
;         \/         \/    \/     \/       \/     \/         \/     \/     \/           \__\         \/     \/      |__|  /__/   

;  rpimenu16_sub.s

;  RasPiC64 - A framework for interfacing the C64 (and the C16/+4) and a Raspberry Pi 3B/3B+
;           - simple menu code running on the 264-series (NTSC/PAL and memory size detection, sending keypress, downloading screen)
;  Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

;  Logo created with http://patorjk.com/software/taag/
 
;  This program is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.

;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
 
;  You should have received a copy of the GNU General Public License
;  along with this program.  If not, see <http://www.gnu.org/licenses/>.

.proc   _wait4IRQ
.export _wait4IRQ
_wait4IRQ:

        pha

        sei

        ; save IRQ vector
        lda $314
        sta $fc
        lda $315
        sta $fd

        ; set new IRQ
        lda #<get_out_with_irq
        sta $314
        lda #>get_out_with_irq
        sta $315

        ; save irq flags
        lda $ff0a
        sta $fe

        ; disable all unnecessary IRQs (keyboard, timers)
        lda #$00
        sta $ff0a

        ; stopping criterion
        lda #$ff
        sta $ff

        ; send keypress to sidekick
        pla
        sta $fd91

        cli

        ; loop until IRQ tells us (via $ff) to get out
loop:
        lda $ff
        bne loop 

        rts

get_out_with_irq:
        sei

        ; restore IRQ vector
        lda $fc
        sta $314
        lda $fd
        sta $315

        ; restore IRQ flags
        lda $fe
        sta $ff0a

        ; signal loop to get out
        lda #$00
        sta $ff

        cli

        jmp $ce0e

.endproc

