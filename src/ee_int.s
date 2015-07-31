;****************************************************************************
;* 
;* ee_int.s
;*
;* - Lectura de las tablas de encriptacion
;*
;*   Fun-o-matic / Noviembre de 2000
;****************************************************************************

#include <CtoASM.inc>

;------------------------------------------------------------------------------   

#define EEARH 0x1f
#define EEARL 0x1e
#define EEDR  0x1d
#define EECR  0x1c
#define EEWE  0x01
#define EERE  0x00

; leer tablas de encriptacion de la eeprom interna
; u08 rb_table1(u08 offset)


 .global rb_table1
 .func   rb_table1

rb_table1:
 cbi     EEARH,0
loop_until_eeprom_ready:
 sbic EECR, EEWE
 rjmp loop_until_eeprom_ready
 out EEARL, rP0
 sbi EECR, EERE
 in rByte, EEDR
 ret
 .endfunc

;------------------------------------------------------------------------------
 .global rb_table2
 .func   rb_table2

; u08 rb_table2(u08 offset)
rb_table2: 
 sbi EEARH,0
 rjmp loop_until_eeprom_ready

 .endfunc
 
 