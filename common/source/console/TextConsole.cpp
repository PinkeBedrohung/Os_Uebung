//----------------------------------------------------------------------
//   $Id: TextConsole.cpp,v 1.7 2005/09/15 17:51:13 nelles Exp $
//----------------------------------------------------------------------
//
//  $Log: TextConsole.cpp,v $
//  Revision 1.6  2005/07/24 17:02:59  nomenquis
//  lots of changes for new console stuff
//
//  Revision 1.5  2005/04/23 20:08:26  nomenquis
//  updates
//
//  Revision 1.4  2005/04/23 18:13:27  nomenquis
//  added optimised memcpy and bzero
//  These still could be made way faster by using asm and using cache bypassing mov instructions
//
//  Revision 1.3  2005/04/23 15:58:32  nomenquis
//  lots of new stuff
//
//  Revision 1.2  2005/04/22 19:43:04  nomenquis
//   more poison added
//
//  Revision 1.1  2005/04/22 17:21:41  nomenquis
//  added TONS of stuff, changed ZILLIONS of things
//
//----------------------------------------------------------------------


#include "TextConsole.h"
#include "Terminal.h"
#include "ArchCommon.h"
#include "panic.h"

#include "Scheduler.h"

#include "arch_keyboard_manager.h"
#include "kprintf.h"

TextConsole::TextConsole(uint32 num_terminals):Console(num_terminals)
{
  uint32 i;
  for (i=0;i<num_terminals;++i)
  {
    Terminal *term = new Terminal(this,consoleGetNumColumns(),consoleGetNumRows());
    terminals_.pushBack(term);
  }
  active_terminal_ = 0;
  name_ = "TxTConsoleThrd";
}

uint32 TextConsole::consoleGetNumRows()const
{
  return 25;
}

uint32 TextConsole::consoleGetNumColumns()const
{
  return 80;
}

void TextConsole::consoleClearScreen()
{
//  if (unlikely(!locked_for_drawing_))
//    panic("Console not locked for drawing, this is REALLY bad");
  
  char *fb = (char*)ArchCommon::getFBPtr();
  uint32 i;
  for (i=0;i<consoleGetNumRows()*consoleGetNumColumns()*2;++i)
  {
    fb[i]=0;
  }
}

uint32 TextConsole::consoleSetCharacter(uint32 const &row, uint32 const&column, uint8 const &character, uint8 const &state)
{
//  if (unlikely(!locked_for_drawing_))
//    panic("Console not locked for drawing, this is REALLY bad");
  
  char *fb = (char*)ArchCommon::getFBPtr();
  fb[(column + row*consoleGetNumColumns())*2] = character;
  fb[(column + row*consoleGetNumColumns())*2+1] = state;
  
  return 0;
}

void TextConsole::consoleScrollUp()
{
//  if (unlikely(!locked_for_drawing_))
//    panic("Console not locked for drawing, this is REALLY bad");
  
  pointer fb = ArchCommon::getFBPtr();
  ArchCommon::memcpy(fb, fb+(consoleGetNumColumns()*2),
    (consoleGetNumRows()-1)*consoleGetNumColumns()*2);
  ArchCommon::bzero(fb+((consoleGetNumRows()-1)*consoleGetNumColumns()*2),consoleGetNumColumns()*2);
}



void TextConsole::consoleSetForegroundColor(FOREGROUNDCOLORS const &color)
{
}

void TextConsole::consoleSetBackgroundColor(BACKGROUNDCOLORS const &color)
{
}

void TextConsole::Run( void )
{
  KeyboardManager * km = KeyboardManager::getInstance();
    
  do 
  {
    uint32 key = km->getKeyFromBuffer();
    if( isDisplayable( key ) )
    {
      key = remap( key );
      terminals_[active_terminal_]->write( key );
      terminals_[active_terminal_]->putInBuffer( key );
    }
    else
      handleKey( key );
  } 
  while(1); // until the end of time

}

void TextConsole::handleKey( uint32 key )
{
  KeyboardManager * km = KeyboardManager::getInstance();
  
  uint32 terminal_selected = (key - KeyboardManager::KEY_F1);
  
  if( terminal_selected < getNumTerminals() && km->isShift() )
  {
    setActiveTerminal(terminal_selected);
    return;
  }
  
  if( key == '\b' )
    terminals_[active_terminal_]->backspace();
  
  return;
}

bool TextConsole::isDisplayable( uint32 key )
{
  return ( ( (key & 127) >= ' ' ) || (key == '\n') || (key == '\b') );
}

bool TextConsole::isLetter( uint32 key )
{
  return ( ( key >= 'a' ) && (key <= 'z') );
}

bool TextConsole::isNumber( uint32 key )
{
  return ( ( key >= '0' ) && (key <= '9') );
}

uint32 TextConsole::remap( uint32 key )
{

  /// TODO: Move this function in terminal and
  ///       implement lookup tables for various
  ///       keyboard layouts
  
  uint32 number_table[] = { ')', '!', '@', '#', '$', 
                            '%', '^', '&', '*', '('  };
  
  KeyboardManager * km = KeyboardManager::getInstance();
  
  if ( isLetter( key ) )
  {
    bool shifted = km->isShift() ^ km->isCaps();
    
    if( shifted )
      key &= ~0x20;  
  }
  
  if ( isNumber( key ) )
  {
    if( km->isShift() )
        key = number_table[ key - '0' ];
    
  }
  
  return key;
}
