#include <Arduino.h>
#include "MenueControl.h"
#include <String.h>
#include "Lite/ApePixelClock.h"

bool playButtonSound = true;
bool isPlaying = false;
bool isLiteMode = false;
bool MenueControl::isMenueMode()
{
    return this->m_isMenueMode;
}

void MenueControl::btnPressed(int btn, int pressState)
{
    playButtonSound = true;
    if (btn == 0 && pressState == 0)
    {
        if (m_isMenueMode)
        {
            this->menueShow(AST_previous);
        }
    }
    else if (btn == 1 && pressState == 0)
    {
        if (m_isMenueMode)
        {
            this->menueShow(AST_enter);
        }
    }
    else if (btn == 1 && pressState == 1)
    {
        if (m_isMenueMode)
        {
            this->menueShow(AST_exit);
        }
        else
        {
            this->menueShow(AST_enter);
        }
    }
    else if (btn == 2 && pressState == 0)
    {
        if (m_isMenueMode)
        {
            this->menueShow(AST_next);
        }
    }
    if (playButtonSound)
    {
        APC.plBegin(18).plByte(3).plCallback();
    }
}

void MenueControl::menueAction(ActionStateType as)
{
    if (as == AST_exit)
    {
        m_isMenueMode = false;
        menueStatus = 0;
    }
    else if (as == AST_next)
    {
        ++menueStatus > 13 ? (menueStatus = 10) : (menueStatus);
    }
    else if (as == AST_previous)
    {
        --menueStatus < 10 ? (menueStatus = 13) : (menueStatus);
    }
    else if (as == AST_enter)
    {
        menueStatus *= 10;
    }
}

void MenueControl::menueShow(ActionStateType as)
{
    switch (menueStatus)
    {
    case 0:
    {
        if (as == AST_enter)
        {
            m_isMenueMode = true;
            menueStatus = 10;
        }
    }
    break;
    case 10:
    {
        this->menueAction(as);
        isPlaying = false;
    }
    break;
    case 100:
    {
        if (as == AST_exit)
        {
            menueStatus /= 10;
            APC.plBegin(27).plCallback();
            isPlaying = false;
        }
        else if (as == AST_next)
        {
            APC.plBegin(25).plCallback();
            playButtonSound = false;
        }
        else if (as == AST_previous)
        {
            APC.plBegin(26).plCallback();
            playButtonSound = false;
        }
        if (as == AST_enter)
        {

            if (!isPlaying)
            {
                APC.plBegin(24).plByte(1).plByte(1).plCallback();
                isPlaying = true;
                playButtonSound = false;
            }
            else
            {
                APC.plBegin(27).plCallback();
                isPlaying = false;
                playButtonSound = false;
            }
        }
    }
    break;
    case 11:
    {
        this->menueAction(as);
    }
    break;
    case 110:
    {
        if (as == AST_exit)
        {
            menueStatus /= 10;
            APC.plBegin(30).plCallback();
            //save
        }
        else if (as == AST_next)
        {
            ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
            apcConfigDef->volume++;
            if (apcConfigDef->volume > 30)
            {
                apcConfigDef->volume = 30;
            }
            APC.plBegin(17).plByte(apcConfigDef->volume).plCallback();
        }
        else if (as == AST_previous)
        {
            ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
            apcConfigDef->volume--;
            if (apcConfigDef->volume < 0)
            {
                apcConfigDef->volume = 0;
            }
            APC.plBegin(17).plByte(apcConfigDef->volume).plCallback();
        }
    }
    break;
    case 12:
    {
        this->menueAction(as);
    }
    break;
    case 120:
    {
        if (as == AST_exit)
        {
            menueStatus /= 10;
            APC.plBegin(30).plCallback();
            //save
        }
        else if (as == AST_next)
        {
            ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
            apcConfigDef->brightness += 8;
            if (apcConfigDef->brightness > 255)
            {
                apcConfigDef->brightness = 255;
            }
            APC.plBegin(13).plByte(apcConfigDef->brightness).plCallback();
        }
        else if (as == AST_previous)
        {
            ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
            apcConfigDef->brightness -= 8;
            if (apcConfigDef->brightness < 0)
            {
                apcConfigDef->brightness = 0;
            }
            APC.plBegin(13).plByte(apcConfigDef->brightness).plCallback();
        }
    }
    break;
    case 13:
    {
        this->menueAction(as);
        isLiteMode = APC.myApcConfigDef()->liteMode;
    }
    break;
    case 130:
    {
        if (as == AST_exit)
        {
            menueStatus /= 10;
            APC.plBegin(31).plByte(isLiteMode ? 1 : 0).plCallback();
            //
            //save  reset
        }
        else if (as == AST_next)
        {
            isLiteMode = !isLiteMode;
        }
        else if (as == AST_previous)
        {
            isLiteMode = !isLiteMode;
        }
    }
    break;
    default:
        break;
    }

    menueRender();
}

void MenueControl::menueRender()
{
    APC.plBegin(9).plCallback();
    switch (menueStatus)
    {
    case 10:
    {
        String menue = "Player";
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
    }
    break;
    case 100:
    {
        //APC.plBegin(3).plCoord(3, 3,false).plByte(3).plColor().plCallback();以3,3为中心半径为3的圆，默认颜色
        APC.plBegin(3).plCoord(15, 3,false).plByte(3).plColor().plCallback();
        APC.plBegin(3).plCoord(15, 4,false).plByte(3).plColor().plCallback();
        APC.plBegin(3).plCoord(16, 3,false).plByte(3).plColor().plCallback();
        APC.plBegin(3).plCoord(16, 4,false).plByte(3).plColor().plCallback();
        APC.plBegin(6).plCoord(15, 2,false).plCoord(15, 5,false).plColor(0, 155, 0).plCallback();
        APC.plBegin(6).plCoord(16, 3,false).plCoord(16, 4,false).plColor(0, 155, 0).plCallback();
    }
    break;
    case 11:
    {
        String menue = "Volume";
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
    }
    break;
    case 110:
    {
        ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
        String menue = String(apcConfigDef->volume);
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
        APC.plBegin(6).plCoord(0, 7,false).plCoord(apcConfigDef->volume, 7,false).plColor(0, 155, 0).plCallback();
    }
    break;
    case 12:
    {
        String menue = "Bright";
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
    }
    break;
    case 120:
    {
        ApcConfigDef *apcConfigDef = APC.myApcConfigDef();
        String menue = String(apcConfigDef->brightness);
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
        int length = map(apcConfigDef->brightness, 0, 255, 0, 31);
        APC.plBegin(6).plCoord(0, 7,false).plCoord(length, 7,false).plColor(0, 155, 0).plCallback();
    }
    break;
    case 13:
    {
        String menue = "OFFLINE";
        APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor().plStr(menue).plCallback();
    }
    break;
    case 130:
    {
        if (isLiteMode)
        {
            String menue = String("Enable");
            APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor(0,255,0).plStr(menue).plCallback();
        }else
        {
            String menue = String("Disable");
            APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(menue.length(), 4, 8), 1,false).plColor(255,0,0).plStr(menue).plCallback();
        }
    }
    break;
    }

    APC.plBegin(8).plCallback();
}
