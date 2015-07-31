;****************************************************************************
;* 
;* FunIO.s
;* Fecha: Abril/Julio 2001
;*   Historia:
;*     2001/04/14. Primera versión basada en SecaRom. Sin comprobar paridad
;*     2001/05/21. Adaptación para Fun-o-matic.
;*
;* Fun-o-matic 2001
;****************************************************************************

;
; ISO7816 (Introducción rápida)
; -----------------------------
;
; La duración de un bit se denomina Elementary Time Unit (etu)
; Para tarjetas que utilizan un reloj externo el etu inicial
; se especifica como 372/fex con fex = frecuencia de trabajo en
; hercios durante el ATR. El propio ATR puede modificar estos 
; valores para las comunicaciones que se realicen a partir de 
; ese momento. Es el etu de trabajo.
;
; La fórmula para el cálculo de un etu de trabajo para tarjetas
; con reloj externo es:
;
; ciclos = (1/D)*(F)
;
; En CSD F = 372 y D = 1 lo que da un etu de 372 ciclos
;
; El marco de transmisión/recepción está compuesto de 
;
; BitDeInicio + 8BitsDeDatos + BitDeParidad -> 10 bits consecutivos
;
; La distancia entre marcos será, de al menos, 12 etus lo que significa
; que se añaden dos etus, como mínimo, a cada marco.
;
; La forma de funcionamiento es la siguiente:
; TX
; Antes de transmitir un marco, el terminal I/O debe estar en alto.
; A continuación se transmite el bit de inicio que es siempre 0.
; Siguen los 8 bits de datos mas el de paridad. El I/O se pone a 1
; y se espera un etu. En ese momento se comprueba el I/O:
;   Si I/O = 1 -> No hubo error en la transmisión. Esperar otro etu
;                 y seguir con otro byte.
;
;   Si I/O = 0 -> Hubo error en la transmisión. Se debe volver a enviar
;                 otra vez los datos después de una espera de, al menos, 
;                 2 etus.
;
; RX 
; El receptor se pone en modo de espera sondeando la I/O.
; Una vez que detecta el bit de inicio, empieza la recepción.
; En el etu 10.5, debe transmitir una señal de error (I/O = 0) si
; la paridad que ha recibido es errónea. La debe mantener un mínimo de 1 etu
; y un máximo de 2 etus. Debe esperar, a continuación, para recibir el dato
; de nuevo. Si no hubo error, debe poner un 1 en la I/O.
;
; Cuando la transmisión se realiza de la tarjeta al receptor, se debe
; guardar un tiempo de seguridad (GuardTime = GT) que indica, en etus,
; el ATR. En el caso de CSD es GT es de un etu.
;
; Falta por implementar la interpretación de los errores de paridad
;

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

          
#define   DATA_DIR   DDRB, 6
#define   DATA_OUT   PORTB, 6
#define   DATA_IN    PINB, 6
#define   DLY(x)    ((x-6)/3)

;********************************************************************
;
; TxToCam : Enviar un byte desde la tarjeta al decodificador
;
;
; El tiempo de transmisión debe ser de
; 13 etus = StartBit + 8 data + ParBit + StopBit + 2 + GT
;
; La distribución, tomando como modelo la ROM de SECA, es:
; 2 etus + StartBit + 8 bits datos + ParBit + StopBit
;
;********************************************************************
;
.global Send
.func Send

Send:
;    push   r18
;    push   r19
    ldi    r19,DLY(738) ; 1
    rcall  Delay        ; + 738 ciclos

    sbi    DATA_DIR     ; 2 Como salida
    clr    __tmp_reg__  ; 1 Para la paridad
    ldi    r18, 0x09    ; 1 Numero de bits ( bit inicio + 8 datos)
    clc                 ; 1 Limpiar el carry para enviar el start bit.

; El algoritmo de envio juega con el estado del bit de carry (C)
;
; Ciclos:
; 
;   Bit 0 __________________  
;   Bit 1 ________________  | 
;                         | |          
_SndLoop:
    brcs   _SndOne      ; 2 1 Estado del carry
    cbi    DATA_OUT     ; 0 2 Mandar un 0
    rjmp   _GoOn        ; 0 2
          
_SndOne:
    sbi    DATA_OUT     ; 2 0 Mandar un 1
    com    __tmp_reg__  ; 1 0 Paridad:conmuta entre 0 y ff

_GoOn:
    rcall  DelayOneEtu  ; + + 363 ciclos
    lsr    dByte        ; 1 1 Pasar el bit 0 al carry
    dec    r18          ; 1 1 Un bit menos
    brne   _SndLoop     ; 2 2

; Totales:   Bit 1 ____ 372 
;            Bit 0 ____   372
;
; 8 bits a 372 y el noveno a 371
;
; Seguimos con la paridad:
;
;   Mandar '0' _____________
;   Mandar '1' ___________  |
;                         | |
    lsr    __tmp_reg__  ; 1 1 Pasar el bit 0 al carry
    brcs   _Par1        ; 2 1 Esto es, hay un numero impar de unos
    cbi    DATA_OUT     ; 0 2 Enviar 0 para la paridad
    rjmp   _SndPar      ; 0 2

_Par1:
    sbi    DATA_OUT     ; 2 0 Enviar 1 para la paridad
    nop                 ; 1 0

_SndPar:
   rcall  DelayOneEtu   ; + + 363 ciclos
   sbi    DATA_OUT      ; 2 2 todo ok = 1

; Totales: Bit 1 ______ 371
;          Bit 0 ________ 371
   
   rcall  DelayOneEtu   ; +   363 ciclos

;    pop  r19
;    pop  r18
   ret                  ; 4
.endfunc

;
; No vamos a comprobar si el receptor nos comunica un error

; Hasta aquí llevamos 744 + 8 * 372 + 371 + 371 = 4462 ciclos
; Saltamos y perderemos otros               367 =  367 ciclos
; Total, unos 13 etus                    Total  = 4829 ciclos
; 

;********************************************************************
;
; RxFromCam : Recibir un byte procedente del decodificador
;
;
; El tiempo de transmisión debe ser de 12 etus
; A los 10.5 etus se debe comunicar si se está de acuerdo
; con los datos recibidos.
; 
; La distribución, tomando como modelo la ROM de SECA, es:
; 0.5 etu + StartBit + 8 bits datos + ParBit + StopBit (1.5 etus)
;
;********************************************************************
;
.global Receive
.func Receive
 
Receive: 
;    push   r18
;    push   r19

    cbi    DATA_DIR     ; Como entrada
    
WaitStartB:
    sbic   DATA_IN      ; Esperar el bit de inicio (un 0)   
    rjmp   WaitStartB   ; 

    ldi    r19,DLY(186) ; 1
    rcall  Delay        ; + 186 -> Esperar 1/2 etu
    ldi    r18, 0x09    ; 1 Bit a recibir->nueve (ya tenemos el bit inicio)
;
; Ciclos:
;
;     Bit 0 ________________  
;     Bit 1 ______________  |  
;                         | |           
GetBit:
    rjmp   aqui         ; 2 2  perder el tiempo
aqui:
    rcall  DelayOneEtu  ; x x  363 ciclos
    clc                 ; 1 1  carry a 0
    sbic   DATA_IN      ; 1 2  si tenemos un 0, saltar
    sec                 ; 1 0  carry a 1
    ror    rByte        ; 1 1  C -> rByte -> C
    dec    r18          ; 1 1  a por el siguiente
    brne   GetBit       ; 2 2 
;
; Totales: Bit 1 ______ 372 
;          Bit 0 ________ 372
;
;           
    rol    rByte        ; 1 1  El primero que recibimos lo acabamos
                        ;      de tirar al carry: lo recuperamos       

; Aquí se cumple que los 9 bits son a 372 c

;
; Hasta ahora llevamos 188 + 9 * 372 = 3536 ciclos
; Le siguen                559 + 369 =  928 ciclos
; Total, 12 etus              Total  = 4464 ciclos
; 
    ldi    r19,DLY(556) ; 1
    rcall  Delay        ; + 556
    sbi    DATA_DIR     ; 2 como salida

; No hemos comprobado la paridad

;
; El bit de stop/idle
;
;IdleState:
   sbi    DATA_OUT      ; 2  todo ok = 1
                            
; Sigue un delay de 363 ciclos
  rcall  DelayOneEtu    ; + 363

;  pop  r19
;  pop  r18
   ret                  ; 4
.endfunc

;*******************************************************************
; Delay : Pierde cierto tiempo
;         Una llamada a esta función hará que se pierdan
;         3 + 4 + 2 + ( ( R19 - 1 ) * 3 ) ciclos, incluyendo RCALL.
;         |   |   |    \_______________/
;         |   |   |            |__________ lazo
;         |   |   |_______________________ última vuelta del lazo
;         |   |___________________________ RET
;         |_______________________________ RCALL
;
; lo que da el #define DLY(x)  ((x-6)/3)
;
; Si se llama a DelayOneEtu hay que sumar el ciclo de carga de r19
;
DelayOneEtu:
    ldi    r19,DLY(362)
Delay:
    dec   r19         
    brne  Delay       
    ret



;------------------------------------------------------------------------------
;
; Funciones de comunicación con el Apollo
; 
; Los ciclos están ajustados para que sean compatibles con el loader original.
;
;------------------------------------------------------------------------------

.global SendA
.func SendA

SendA:
    sbi   PORTB, 6
    cbi   PORTB, 6
    ldi   r18, 0x08     ; 1 1 Numero de bits ( bit inicio + 8 datos)
    rjmp  _SndLoopA     ; 2 2 Solo perder el tiempo
    
; El algoritmo de envio juega con el estado del bit de carry (C)
; El tiempo está ajustado para que sea compatible con el loader
; original.
;
; Ciclos:
; 
;   Bit 0 __________________  
;   Bit 1 ________________  | 
;                         | |          
_SndLoopA:
    lsl    dByte        ; 1 1 Pasar el bit 0 al carry
    brcs   _SndOneA     ; 2 1 Estado del carry
    cbi    DATA_OUT     ; 0 2 Mandar un 0
    rjmp   _GoOnA       ; 0 2
          
_SndOneA:
    sbi    DATA_OUT     ; 2 0 Mandar un 1
    nop                 ; 1 0

_GoOnA:
    rjmp  _GoOnA1       ; 2 2 Solo perder el tiempo
_GoOnA1:
    rjmp  _GoOnA2       ; 2 2 Solo perder el tiempo
_GoOnA2:                ;     Aquí se cumplen los primeros 13 ciclos 
                        ;     y los 18 del loop
    rjmp  _GoOnA3       ; 2 2 Solo perder el tiempo
_GoOnA3:
    rjmp  _GoOnA4       ; 2 2 Solo perder el tiempo
_GoOnA4:
    nop                 ; 1 1 Solo perder el tiempo
    dec    r18          ; 1 1 Un bit menos
    brne   _SndLoopA    ; 2 2

; Totales:   Bit 1 ____  18 
;            Bit 0 ____    18 
;

    cbi   PORTB, 6
    ret
.endfunc
  

;------------------------------------------------------------------------------

.global ReceiveA
.func   ReceiveA

ReceiveA:
    sbi    PORTB, 6
    ldi    r18, 0x08    ; 1 
    rjmp   GetBitA      ; 2
;
; El tiempo está ajustado para que sea compatible con el loader
; original.
; Ciclos:
;
;     Bit 0 ________________  
;     Bit 1 ______________  |  
;                         | |           
GetBitA:
    rjmp   GetBitA0     ; 2 2
GetBitA0:
    clc                 ; 1 1  carry a 0
    sbic   PINB, 5      ; 1 2  si tenemos un 0, saltar
    sec                 ; 1 0  carry a 1
    rol    rByte        ; 1 1  rByte <- C
    rjmp   GetBitA1     ; 2 2  Solo perder el tiempo
GetBitA1:
    rjmp   GetBitA2     ; 2 2  Solo perder el tiempo
GetBitA2:
    dec    r18          ; 1 1  a por el siguiente
    brne   GetBitA      ; 2 2 
;
; Totales: Bit 1 _______ 12 
;          Bit 0 _______   12
;
;           
   cbi     PORTB, 6
   ret                  ; 4
.endfunc