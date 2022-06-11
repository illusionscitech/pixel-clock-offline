#ifndef __MenueControl_H
#define __MenueControl_H

#include <Arduino.h>

enum ActionStateType
{
      AST_enter = 0,
      AST_exit,
      AST_previous,
      AST_next
};

class MenueControl
{
  private:
    bool m_isMenueMode = false;

    uint8 menueStatus = 0; 

    void menueShow(ActionStateType as);

    void menueRender();

    void menueAction(ActionStateType as);

  public:

    bool isMenueMode();

    void btnPressed(int btn,int pressState);

};

#endif // __MenueControl_H
