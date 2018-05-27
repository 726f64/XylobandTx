/* 
 * File:   LCD_Module.h
 * Author: General
 *
 * Created on 10 August 2016, 11:29
 */

#ifndef LCD_MODULE_H
#define	LCD_MODULE_H

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif

void initialLCD(void);
void LCD_busy(void);
void LCD_write(const char*,bool);
void LCD_clear(void);
void LCD_home(void);
void LCD_Instruction(char );
void LCD_Data(char );
void LCD_DDRAM(char );


#endif	/* LCD_MODULE_H */

