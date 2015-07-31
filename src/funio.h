/*****************************************************************************
*
* FunIO.h
*
* Rutinas para la entrada/salida de datos y lectura de tablas
*
****************************************************************************/

#ifndef __FUNIO_H__
#define __FUNIO_H__

#include "types.h"


// I/O CAM<->FunCard
void Send(u08);
u08  Receive(void);

// I/O CAM<->FunCard (Apollo)
void SendA(u08);
u08  ReceiveA(void);

// Leer tablas criptográficas de la Eeprom Interna
 u08 rb_table1(u08);
 u08 rb_table2(u08);
 
#endif

