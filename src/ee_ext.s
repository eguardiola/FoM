;****************************************************************************
;*
;* ee_ext.s
;* Acceso a la eeprom externa de la FunCard
;*
;*
;* Fun-o-matic
;****************************************************************************

#include <CtoASM.inc>

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#  define dByte   rP0
# else
#  define dByte   rP1
# endif /* GNUC < 2.96 */


#define PORTB 0x18
#define DDRB  0x17
#define PINB  0x16

#define PORTD 0x12
#define DDRD  0x11
#define PIND  0x10
#define PB6   0x06

;------------------------------------------------------------------------
; void extee_init(void);

        .global extee_init
        .func extee_init


extee_init:
	cbi	 PORTB,	0
	cbi	 PORTB,	1
	sbi	 DDRB, 0
	sbi	 DDRB, 1
	ret

        .endfunc

;------------------------------------------------------------------------
; void extee_startCondition(void);

        .global extee_startCondition
        .func extee_startCondition


;
;  El bit de inicio es la transicion de alto a bajo en SDA mientras
;  SCL esta en alto
;
;           __________
;          /          \
;  SCL ___/            \_____
;      __________
;                \
;  SDA            \_____
;
extee_startCondition:

	sbi	 PORTB,	0 ; low SDA
	sbi	 PORTB,	1 ; high SCL
	nop
	nop
	cbi	 PORTB,	0 ; high SDA
	nop
	nop
	cbi	 PORTB,	1 ; low SCL
	ret

        .endfunc

;------------------------------------------------------------------------

; void extee_stopConditionr(void);

        .global extee_stopCondition
        .func extee_stopCondition

;   El bit de parada es la transicion de bajo a alto en SDA mientras
;   SCL esta en alto
;
;           ____________
;          /
;  SCL ___/
;                 ______
;                /
;  SDA _________/
;

extee_stopCondition:
	sbi	 PORTB,	1 ; high SCL
	nop
	nop
	sbi	 PORTB,	0 ; low SDA
	nop
	nop
	ret

        .endfunc

;------------------------------------------------------------------------

;u08 extee_sendByte(u08 controlByte);

        .global extee_sendByte
        .func extee_sendByte

extee_sendByte:
; push  r18
	ldi	  r18, 8	    ; 8 bits

sc_loop:
	cpi	  dByte, 0	  ; Averiguamos que tengo que enviar
	brge	EnviaEe1	  ; Si es un cero, prepáralo.
	sbi	  PORTB,	0   ; Envia un uno (high_sda)

EnviaEe1:
	sbi	  PORTB,	1	  ; pulso de reloj, up SCL
	nop
	nop
	cbi	  PORTB,	1	  ; pulso de reloj, low en el SCL
	cbi	  PORTB,	0
	lsl	  dByte		    ; un bit menos
	dec	  r18		      ; un bit menos
	brne	sc_loop	    ; cierra bucle
	cbi	  DDRB, 0
	sbi	  PORTB,	0
	sbi	  PORTB,	1
	nop
	nop
	in	  dByte, PINB	;	ACK de la eeprom
	andi	dByte, 1
	cbi	  PORTB,	1
	cbi	  PORTB,	0
	sbi	  DDRB, 0
	mov	  rByte,dByte ; devolver modificacion del byte entrado
;	pop	  r18		      ;intentaremos eliminarlos luego ;)
	ret

  .endfunc

;------------------------------------------------------------------------

; u08 extee_receiveByte(void);

        .global extee_receiveByte
        .func extee_receiveByte

extee_receiveByte:
;	push 	 r18
	cbi	  DDRB, 0	  ; PORTB en input
	sbi	  PORTB,0
	ldi	  r18, 8

rb_loop:
	lsl	  rByte		  ; rota rByte
	sbi	  PORTB,1	  ; levanta el SCL
	nop
	nop
	sbic	PINB,0		; lee
	ori	  rByte,1	  ; añade un 1 o 0 al rByte en función de lo leído.
	cbi	  PORTB,1	  ; abajo el SCL
	dec	  r18		    ; siguiente bit
	brne  rb_loop
	cbi	  PORTB,0	  ; otro pulso en el SDA ?
	sbi	  DDRB, 0

;	pop 	 r18
	ret

  .endfunc
