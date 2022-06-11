#include "ApePixelClock.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <vector>
#include "MenueControl/MenueControl.h"

RTC_DS1307 *m_rtc;
ApcConfigDef *m_apcConfigDef;
std::vector<ApcEffectDef *> apcEffects;
std::vector<ApcScheduleCallbackDef *> apcScheduleCallbacks;
MenueControl myMenue;

int apcEffectPointer;
unsigned long preCheckTime = 0;
unsigned long callbackCheckTime = 0;
int weatherInfo[4] = {0, 0, 0, 0};
int inDoorInfo[3] = {0, 0, 0};
float ldrInfo = 0;
bool autoChange = true;
bool isAlarming = false;
bool stopAlarm = false;
ApcScheduleCallbackDef *alarmDisable_schedule = NULL;

uint32 biliSubscriberCount = 0;
uint32 youTubeSubscriberCount = 0;

byte payload[256];
unsigned int payLoadPointer = 0;

void timeShowEffect(unsigned int areaCount, unsigned int frameCount)
{
  DateTime now = m_rtc->now();
  if (frameCount < 5)
  {
    int r1 = rand()%(255)+255;    //时间及日期颜色随机数显示
    int g1 = rand()%(255)+255;    //产生[m,n]范围内的随机数num，可用：int num=rand()%(n-m+1)+m;
    int b1 = rand()%(255)+255;
    APC.plBegin(0).plCoord(7, 1).plColor(r1,g1,b1);
    char timeChar[6];
    //Serial.println(frameCount);
    if (frameCount % 2 == 0)   //frameCount为偶数
    {
      sprintf(timeChar, "%02d:%02d", now.hour(), now.minute());
    }
    else
    {
      sprintf(timeChar, "%02d %02d", now.hour(), now.minute());
    }
    APC.plStr(String(timeChar)).plCallback();
  }
  else
  {
    int r1 = rand()%(255)+255;
    int g1 = rand()%(255)+255;
    int b1 = rand()%(255)+255;
    APC.plBegin(0).plCoord(6, 1).plColor(r1,g1,b1);
    char timeChar[6];
    sprintf(timeChar, "%02d/%02d", now.month(), now.day());
    APC.plStr(String(timeChar)).plCallback();
  }
  int w = now.dayOfTheWeek() - 1;
  if (w < 0)
    w = 6;
  for (int i = 0; i < 7; i++)
  {
    APC.plBegin(6).plCoord(3 + 4 * i, 7).plCoord(3 + 4 * i + 2, 7);
    if (w == i)
    {
      APC.plColor(25,63,255);   //蓝紫
    }
    else
    {
      APC.plColor(175, 255, 253);   //青蓝
    }
    APC.plCallback();
  }
}

const uint32 biliColorArr[2] ICACHE_RODATA_ATTR = {0x000000, 0x00A1F1};
const uint32 biliPixels[16] ICACHE_RODATA_ATTR =
    {
        0x00010000, 0x00000100,
        0x00000100, 0x00010000,
        0x00010101, 0x01010100,
        0x01000000, 0x00000001,
        0x01000100, 0x00010001,
        0x01000100, 0x00010001,
        0x01000000, 0x00000001,
        0x00010101, 0x01010100};

void bilibiliEffect(unsigned int areaCount, unsigned int frameCount)
{
  if (areaCount == 0)
  {
    APC.drawColorIndexFrame(biliColorArr, 8, 8, biliPixels);
  }
  else if (areaCount == 1)
  {
    String num = APC.subscriberCountFormat(biliSubscriberCount);
    APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
    APC.plBegin(6).plCoord(10, 7,false).plCoord(29, 7,false).plColor(0, 161, 241).plCallback();
  }
}

const uint32 youtubeColorArr[3] ICACHE_RODATA_ATTR = {0x000000, 0xFF0000, 0xFFFFFF};
const uint32 youtubePixels[16] ICACHE_RODATA_ATTR =
    {
        0x00000000, 0x00000000,
        0x00010101, 0x01010100,
        0x01010102, 0x01010101,
        0x01010102, 0x02010101,
        0x01010102, 0x02010101,
        0x01010102, 0x01010101,
        0x00010101, 0x01010100,
        0x00000000, 0x00000000};

void youtubeEffect(unsigned int areaCount, unsigned int frameCount)
{
  if (areaCount == 0)
  {
    APC.drawColorIndexFrame(youtubeColorArr, 8, 8, youtubePixels);
  }
  else if (areaCount == 1)
  {
    String num = APC.subscriberCountFormat(youTubeSubscriberCount);
    APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
  }
}

const uint32 weatherColorArr[3] ICACHE_RODATA_ATTR = {0x000000, 0x1ab5ed, 0xffc106}; //蓝 黄
const uint32 weatherPixels[16] ICACHE_RODATA_ATTR =
    {
        0x00000000, 0x02020000,
        0x00000002, 0x02020200,
        0x00000202, 0x02020202,
        0x00000101, 0x02020202,
        0x00010101, 0x01020200,
        0x01010101, 0x01010000,
        0x01010101, 0x01010100,
        0x00000000, 0x00000000};

void weatherEffect(unsigned int areaCount, unsigned int frameCount)
{
  if (areaCount == 0)
  {
    APC.drawColorIndexFrame(weatherColorArr, 8, 8, weatherPixels);
  }
  else if (areaCount == 1)
  {
    if (frameCount < 4)
    {
      String num = String(weatherInfo[1]) + "'C";
      APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
    }
    else if(frameCount < 7)
    {
      String num = String(weatherInfo[2]) + " %";
      APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
    }
    else if(frameCount < 8)
    {
      String num = String(weatherInfo[3]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7);
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      }
    else if(frameCount < 9)
    {
      String num = String(weatherInfo[3]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 1 ;
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      }
    else if(frameCount < 10)
    {
      String num = String(weatherInfo[3]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 2 ;
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      }
    else
    {
      //Serial.println(weatherInfo[3]);
      String num = String(weatherInfo[3]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 3 ;
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
    }
  }
}

const uint32 indoorColorArr[3] ICACHE_RODATA_ATTR = {0x000000, 0xc40c0c, 0xFFFFFF};
const uint32 indoorPixels[16] ICACHE_RODATA_ATTR =
    {
        0x00000002, 0x00000000,
        0x00000200, 0x02000000,
        0x00000201, 0x02000000,
        0x00000201, 0x02000000,
        0x00000201, 0x02000000,
        0x00020101, 0x01020000,
        0x00020101, 0x01020000,
        0x00000202, 0x02000000};

void indoorEffect(unsigned int areaCount, unsigned int frameCount)
{
  if (areaCount == 0)
  {
    APC.drawColorIndexFrame(indoorColorArr, 8, 8, indoorPixels);
  }
  else if (areaCount == 1)
  {
    if (frameCount < 4)
    {
      String num = String(inDoorInfo[0]) + "'C";
      APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
    }
    else if(frameCount < 7)
    {
      String num = String(inDoorInfo[1]) + " %";
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
      APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1).plColor().plStr(num).plCallback();
      
    }
    else if(frameCount < 8)
    {
      String num = String(inDoorInfo[2]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7) ;
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      
      }
    else if(frameCount < 9)
    {
      String num = String(inDoorInfo[2]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 1 ;
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      
      }
      else if(frameCount < 10)
    {
      String num = String(inDoorInfo[2]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 2 ;
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      
      }
    else
    {
      String num = String(inDoorInfo[2]) + "hpa";
      int enter = APC.textCenterX(num.length(), 4, 7)- 3 ;
      APC.plBegin(0).plCoord(5, 4,false).plColor(196,12,12).plStr("in").plCallback();
      APC.plBegin(0).plCoord((uint16_t)enter, 1).plColor().plStr(num).plCallback();
      
      // APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 7), 1).plColor().plStr(num).plCallback();
    }
  }
}

const uint32 cddColorArr[3] ICACHE_RODATA_ATTR = {0x000000, 0x1857ff, 0xFFFFFF};
const uint32 cddPixels[16] ICACHE_RODATA_ATTR =
    {
        0x02020202, 0x02020200,
        0x00020100, 0x01020000,
        0x00000201, 0x02000000,
        0x00000002, 0x00000000,
        0x00000002, 0x00000000,
        0x00000200, 0x02000000,
        0x00020001, 0x00020000,
        0x02020202, 0x02020200};

void countdownDayEffect(unsigned int areaCount, unsigned int frameCount)
{
  DateTime now = DateTime(m_rtc->now().year(), m_rtc->now().month(), m_rtc->now().day());
  String target(m_apcConfigDef->cdd_date);
  int year = target.substring(0, 4).toInt();
  int month = target.substring(5, 7).toInt();
  int day = target.substring(8, 10).toInt();
  DateTime tar = DateTime(year, month, day);
  TimeSpan diff = TimeSpan(tar.unixtime() - now.unixtime());
  int diffDay = diff.days();
  if (areaCount == 0)
  {
    String num = String(diffDay);
    APC.plBegin(0).plCoord((uint16_t)APC.textCenterX(num.length(), 4, 6), 1);
    if (diffDay <= 0)
    {
      APC.plColor(255, 0, 0);
    }
    else
    {
      APC.plColor();
    }
    APC.plStr(num).plCallback();
  }
  else if (areaCount == 1)
  {
    APC.drawColorIndexFrame(cddColorArr, 8, 8, cddPixels);
  }
  else
  {
    if (diffDay <= 0 && !isAlarming)
    {
      APC.plBegin(18).plByte(4).plCallback();
    }
  }
}

void rainEffect(unsigned int areaCount, unsigned int frameCount)
{
        for(int i=0;i<=31;i++){
          int numa=rand()%(8)+0;
          int r1 = rand()%(255)+255;
          int g1 = rand()%(255)+255;
          int b1 = rand()%(255)+255;
          APC.plBegin(6).plCoord(i, numa,false).plCoord(i, numa,false).plColor(r1, g1, b1).plCallback();
          // int numb=rand()%(8)+0;
          // APC.plBegin(6).plCoord(i, numb,false).plCoord(i, numb,false).plColor(0, 161, 241).plCallback();
        }
}


void tetrisEffect(unsigned int areaCount, unsigned int frameCount)
{
  int a=0;
  int b=0;
  int c=0;
  if (frameCount < 1){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 2){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 3){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 4){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 5){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 6){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else if (frameCount < 7){
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  else {
    //方块 黄色
    APC.plBegin(6).plCoord(a, b+frameCount,false).plCoord(a+1, b+frameCount,false).plColor(255, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a, b+1+frameCount,false).plCoord(a+1, b+1+frameCount,false).plColor(255, 255, 0).plCallback();
    //横条 浅蓝
    APC.plBegin(6).plCoord(a+3, b+frameCount,false).plCoord(a+6, b+frameCount,false).plColor(0, 255, 255).plCallback();
    //L 蓝
    APC.plBegin(6).plCoord(a+8, b+frameCount,false).plCoord(a+8, b+frameCount,false).plColor(0, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+8, b+1+frameCount,false).plCoord(a+10, b+1+frameCount,false).plColor(0, 0, 204).plCallback();
    //L 橙
    APC.plBegin(6).plCoord(a+14, b+frameCount,false).plCoord(a+14, b+frameCount,false).plColor(255, 128, 0).plCallback();
    APC.plBegin(6).plCoord(a+12, b+1+frameCount,false).plCoord(a+14, b+1+frameCount,false).plColor(255, 128, 0).plCallback();
    //s 绿
    APC.plBegin(6).plCoord(a+17, b+frameCount,false).plCoord(a+18, b+frameCount,false).plColor(0, 255, 0).plCallback();
    APC.plBegin(6).plCoord(a+16, b+1+frameCount,false).plCoord(a+17, b+1+frameCount,false).plColor(0, 255, 0).plCallback();
    //倒T 紫
    APC.plBegin(6).plCoord(a+21, b+frameCount,false).plCoord(a+21, b+frameCount,false).plColor(102, 0, 204).plCallback();
    APC.plBegin(6).plCoord(a+20, b+1+frameCount,false).plCoord(a+22, b+1+frameCount,false).plColor(102, 0, 204).plCallback();
    //L hong
    APC.plBegin(6).plCoord(a+24, b+frameCount,false).plCoord(a+24, b+frameCount,false).plColor(255, 0, 0).plCallback();
    APC.plBegin(6).plCoord(a+24, b+1+frameCount,false).plCoord(a+26, b+1+frameCount,false).plColor(255, 0, 0).plCallback();
    //s pick
    APC.plBegin(6).plCoord(a+29, b+frameCount,false).plCoord(a+30, b+frameCount,false).plColor(255, 0, 127).plCallback();
    APC.plBegin(6).plCoord(a+28, b+1+frameCount,false).plCoord(a+29, b+1+frameCount,false).plColor(255, 0, 127).plCallback();
  }
  
  
  

  // //方块 黄色
  // APC.plBegin(6).plCoord(0, 0,false).plCoord(0, 1,false).plColor(255, 255, 0).plCallback();
  // APC.plBegin(6).plCoord(1, 0,false).plCoord(1, 1,false).plColor(255, 255, 0).plCallback();
  // //横条 浅蓝
  // APC.plBegin(6).plCoord(3, 0,false).plCoord(6, 0,false).plColor(0, 255, 255).plCallback();
  // //L 蓝
  // APC.plBegin(6).plCoord(8, 0,false).plCoord(8, 0,false).plColor(0, 0, 204).plCallback();
  // APC.plBegin(6).plCoord(8, 1,false).plCoord(10, 1,false).plColor(0, 0, 204).plCallback();
  // //L 橙
  // APC.plBegin(6).plCoord(14, 0,false).plCoord(14, 0,false).plColor(255, 128, 0).plCallback();
  // APC.plBegin(6).plCoord(12, 1,false).plCoord(14, 1,false).plColor(255, 128, 0).plCallback();
  // //s 绿
  // APC.plBegin(6).plCoord(16, 0,false).plCoord(17, 0,false).plColor(0, 255, 0).plCallback();
  // APC.plBegin(6).plCoord(15, 1,false).plCoord(16, 1,false).plColor(0, 255, 0).plCallback();
  // //倒T 紫
  // APC.plBegin(6).plCoord(20, 0,false).plCoord(20, 0,false).plColor(102, 0, 204).plCallback();
  // APC.plBegin(6).plCoord(19, 1,false).plCoord(21, 1,false).plColor(102, 0, 204).plCallback();
}

const String bilibili_API = "https://api.bilibili.com/x/relation/stat?vmid=";
void updateBilibiliSubscriberCount()
{
  String url = bilibili_API + String(BILIBILI_UID);
  int errCode = 0;
  const String &res = APC.httpsRequest(url, &errCode);
  if (errCode == 0)
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(res);
    biliSubscriberCount = json["data"]["follower"].as<uint32>();
  }
}

const String youtube_API = "https://www.googleapis.com/youtube/v3/channels?part=statistics";
void updateYoutubeSubscriberCount()
{
  String url = youtube_API + "&id=" + String(YOUTUBE_CHANNEL) + "&key=" + String(YOUTUBE_APIKEY);
  int errCode = 0;
  const String &res = APC.httpsRequest(url, &errCode);
  if (errCode == 0)
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(res);
    youTubeSubscriberCount = json["items"][0]["statistics"]["subscriberCount"].as<uint32>();
  }
}
const String weather_API = "https://devapi.heweather.net/v7/weather/now?gzip=n&";
void updateWeatherInfo()
{
  String url = weather_API + "location=" + String(WEATHER_CITY) + "&key=" + String(WEATHER_APIKEY);
  int errCode = 0;
  const String &res = APC.httpsRequest(url, &errCode);
  if (errCode == 0)
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(res);
    JsonObject &nowObj = json["now"].as<JsonObject>();
    weatherInfo[0] = nowObj["icon"].as<int>();
    weatherInfo[1] = nowObj["temp"].as<int>();
    weatherInfo[2] = nowObj["humidity"].as<int>();
    weatherInfo[3] = nowObj["pressure"].as<int>();
    //Serial.println(weatherInfo[3]);
  }
}

void updateSensorInfo()
{
  APC.plBegin(12).plCallback();
}

void ApePixelClock::ramCheck(const char *info)
{
  Serial.printf("%s: S:%d,H:%d,M:%d \n", info, ESP.getFreeContStack(), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
}

void schedule_callback(int callbackId)
{
  switch (callbackId)
  {
  case 201:
    APC.checkAlarm();
    break;
  case 202:
    APC.removeApcScheduleCallback(alarmDisable_schedule);
    isAlarming = false;
    stopAlarm = false;
    break;
  case 203:
    APC.checkLDR();
    break;
  case 1:

    break;
  case 2:
    updateBilibiliSubscriberCount();
    break;
  case 3:
    updateYoutubeSubscriberCount();
    break;
  case 4:
    updateWeatherInfo();
    break;
  case 5:
    updateSensorInfo();
    break;
  default:
    break;
  }
}

void ApePixelClock::addApcEffects()
{
  ApcEffectDef *apcEffect = NULL;

#if (TIME_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 1;
  apcEffect->updateTime = 0;
  apcEffect->autoChangeTime = 10000;   //每个画面显示时间
  apcEffect->areaDef[0] = {0, 0, 32, 8, 10, 1000};
  this->addApcEffect(apcEffect);
#endif

#if (BILIBILI_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 2;
  apcEffect->updateTime = 900000;
  apcEffect->autoChangeTime = 4000;    
  apcEffect->areaDef[0] = {0, 0, 8, 8, 1, 0};
  apcEffect->areaDef[1] = {8, 0, 24, 8, 1, 0};
  this->addApcEffect(apcEffect);
#endif

#if (YOUTUBE_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 3;
  apcEffect->updateTime = 900000;
  apcEffect->autoChangeTime = 4000;
  apcEffect->areaDef[0] = {0, 0, 8, 8, 1, 0};
  apcEffect->areaDef[1] = {8, 0, 24, 8, 1, 0};
  this->addApcEffect(apcEffect);
#endif

#if (WEATHER_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 4;
  apcEffect->updateTime = 900000;
  apcEffect->autoChangeTime = 10000;
  apcEffect->areaDef[0] = {0, 0, 8, 8, 1, 0};
  apcEffect->areaDef[1] = {8, 0, 24, 8, 10, 1000};
  this->addApcEffect(apcEffect);
#endif

#if (INDOOR_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 5;
  apcEffect->updateTime = 10000;
  apcEffect->autoChangeTime = 10000;
  apcEffect->areaDef[0] = {0, 0, 8, 8, 1, 0};
  apcEffect->areaDef[1] = {8, 0, 24, 8, 10, 1000};
  this->addApcEffect(apcEffect);
#endif

#if (COUNTDOWN_DAY_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 6;
  apcEffect->autoChangeTime = 5000;
  apcEffect->areaDef[0] = {8, 0, 24, 8, 10, 1000};
  apcEffect->areaDef[1] = {0, 0, 8, 8, 1, 0};
  apcEffect->areaDef[2] = {0, 0, 0, 0, 4, 3000};
  this->addApcEffect(apcEffect);
#endif

#if (RAIN_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 7;
  apcEffect->autoChangeTime = 5000;
  apcEffect->areaDef[0] = {0, 0, 32, 8, 100, 100};
  this->addApcEffect(apcEffect);
#endif

#if (TETRIS_SHOW == 1)
  apcEffect = new ApcEffectDef();
  memset(apcEffect, 0, sizeof(ApcEffectDef));
  apcEffect->effectId = 8;
  apcEffect->autoChangeTime = 7000;
  apcEffect->areaDef[0] = {0, 0, 32, 8, 10, 1000};
  // apcEffect->areaDef[0] = {0, 0, 8, 8, 10, 1000};
  // apcEffect->areaDef[1] = {8, 0, 8, 8, 100, 100};
  // apcEffect->areaDef[2] = {16, 0, 8, 8, 100, 100};
  // apcEffect->areaDef[3] = {24, 0, 8, 8, 100, 100};
  // apcEffect->currentChangeTime = 100;
  this->addApcEffect(apcEffect);
#endif
}


void ApePixelClock::addApcEffect(ApcEffectDef *apcEffect)
{
  int apcEffectAreaCount = 0;
  for (int i = 0; i < MAX_APCEffECTAREA_COUNT; i++)
  {
    ApcEffectAreaDef &apcEffectArea = apcEffect->areaDef[i];
    if (apcEffectArea.frameCount > 0)
    {
      apcEffectArea.currentFrameCount = 0;
      apcEffectArea.currentRefreshTime = 0;
      apcEffectAreaCount++;
    }
  }
  apcEffect->areaCount = apcEffectAreaCount;
  apcEffect->currentAreaIndex = 0;
  apcEffect->currentChangeTime = apcEffect->autoChangeTime;
  apcEffects.push_back(apcEffect);

  if (apcEffect->updateTime > 0)
  {
    this->addApcScheduleCallback(apcEffect->effectId, apcEffect->updateTime);
  }
}

ApcScheduleCallbackDef *ApePixelClock::addApcScheduleCallback(int callbackId, unsigned long callbackTime, bool callbackNow)
{
  ApcScheduleCallbackDef *apcScheduleCallback = new ApcScheduleCallbackDef();
  apcScheduleCallback->callbackId = callbackId;
  apcScheduleCallback->callbackTime = callbackTime;
  if (callbackNow)
  {
    apcScheduleCallback->currentRefreshTime = 0;
  }
  else
  {
    apcScheduleCallback->currentRefreshTime = callbackTime;
  }

  apcScheduleCallbacks.push_back(apcScheduleCallback);
}

void ApePixelClock::removeApcScheduleCallback(ApcScheduleCallbackDef *callbackDef)
{
  for (auto x = apcScheduleCallbacks.begin(); x != apcScheduleCallbacks.end();)
  {
    if (callbackDef == *x)
    {
      apcScheduleCallbacks.erase(x);
      delete callbackDef;
      callbackDef = NULL;
      break;
    }
    ++x;
  }
}

void apcEffect_callback(int effectId, unsigned int areaCount, unsigned int frameCount)
{
  switch (effectId)
  {
  case 1:
    timeShowEffect(areaCount, frameCount);
    break;
  case 2:
    bilibiliEffect(areaCount, frameCount);
    break;
  case 3:
    youtubeEffect(areaCount, frameCount);
    break;
  case 4:
    weatherEffect(areaCount, frameCount);
    break;
  case 5:
    indoorEffect(areaCount, frameCount);
    break;
  case 6:
    countdownDayEffect(areaCount, frameCount);
    break;
  case 7:
    rainEffect(areaCount, frameCount);
    break;
  case 8:
    tetrisEffect(areaCount, frameCount);
    break;
  default:
    break;
  }
}

ApePixelClock::ApePixelClock()
{
}

void ApePixelClock::apcSetup()
{
  this->upgradeTime();
  callbackCheckTime = millis();
  preCheckTime = millis();
  this->effectDisplayInit();

  APC.plBegin(13).plByte(m_apcConfigDef->brightness).plCallback();
  APC.plBegin(17).plByte(m_apcConfigDef->volume).plCallback();
}

void ApePixelClock::apcLoop()
{
  if (!myMenue.isMenueMode())
  {
    this->renderCheck();
    this->apcCallbackAction();
  }
}

void ApePixelClock::checkAlarm()
{
  if (m_apcConfigDef->alarm_enable && !isAlarming)
  {
    String alarmTarget = String(m_apcConfigDef->alarm_time);
    DateTime now = m_rtc->now();
    char timeChar[6];
    sprintf(timeChar, "%02d:%02d", now.hour(), now.minute());
    String alarmNow = String(timeChar);
    if (alarmTarget == alarmNow)
    {
      isAlarming = true;
      APC.plBegin(18).plByte(11).plCallback();
      alarmDisable_schedule = this->addApcScheduleCallback(202, 60000, false);
    }
  }
}

void ApePixelClock::checkLDR()
{
  updateSensorInfo();
  //Serial.print("光强 ");
  //Serial.println(ldrInfo);
  if (ldrInfo < 0)
  {
  }
  else if (ldrInfo < 10)   //暗光环境极低亮度显示
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (3 / 100.0)).plCallback();
  }
  else if (ldrInfo < 20)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (6 / 100.0)).plCallback();
  }
  else if (ldrInfo < 30)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (10 / 100.0)).plCallback();
  }
  else if (ldrInfo < 60)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (20 / 100.0)).plCallback();
  }
  else if (ldrInfo < 150)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (30 / 100.0)).plCallback();
  }
  else if (ldrInfo < 500)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (40 / 100.0)).plCallback();
  }
  else if (ldrInfo < 1000)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (50 / 100.0)).plCallback();
  }
  else if (ldrInfo < 2000)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (60 / 100.0)).plCallback();
  }
  else if (ldrInfo < 5000)
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (80 / 100.0)).plCallback();
  }
  else
  {
    APC.plBegin(13).plByte(m_apcConfigDef->brightness * (100 / 100.0)).plCallback();
  }
}

ApcConfigDef *ApePixelClock::myApcConfigDef()
{
  return m_apcConfigDef;
}

void ApePixelClock::renderCheck()
{
  if (apcEffectPointer >= int(apcEffects.size()))
    return;
  ApcEffectDef *apcEffect = apcEffects[apcEffectPointer];
  

  if (autoChange)
  {
    if (apcEffect->autoChangeTime > 0)
    {
      int diff = millis() - preCheckTime;
      if (diff > 0)
      {
        if (apcEffect->currentChangeTime > diff)
        {
          apcEffect->currentChangeTime -= diff;
          
        }
        else
        {
          this->clear();
          this->apcEffectChangeAction();
        }
      }
    }
  }
  apcEffect = apcEffects[apcEffectPointer];
  int areaCount = apcEffect->areaCount;
  for (int i = 0; i < areaCount; i++)
  {
    apcEffect->currentAreaIndex = i;
    ApcEffectAreaDef &apcEffectArea = apcEffect->areaDef[i];
    if (apcEffectArea.frameRefreshTime > 0)
    {
      int diff = millis() - preCheckTime;
      if (diff > 0)
      {
        if (apcEffectArea.currentRefreshTime > diff)
        {
          apcEffectArea.currentRefreshTime -= diff;
        }
        else
        {
          this->renderAction(apcEffect);
          apcEffectArea.currentFrameCount++;
          if (apcEffectArea.currentFrameCount >= apcEffectArea.frameCount)
          {
            apcEffectArea.currentFrameCount = 0;
          }
          apcEffectArea.currentRefreshTime = apcEffectArea.frameRefreshTime;
        }
      }
    }
  }
  preCheckTime = millis();
  this->show();
}

void ApePixelClock::apcCallbackAction()
{
  for (int i = 0; i < (int)apcScheduleCallbacks.size(); i++)
  {
    ApcScheduleCallbackDef *apcScheduleCallback = apcScheduleCallbacks[i];
    if (apcScheduleCallback->callbackTime > 0)
    {
      int diff = millis() - callbackCheckTime;
      if (diff > 0)
      {
        if (apcScheduleCallback->currentRefreshTime > diff)
        {
          apcScheduleCallback->currentRefreshTime -= (millis() - callbackCheckTime);
        }
        else
        {
          schedule_callback(apcScheduleCallback->callbackId);
          apcScheduleCallback->currentRefreshTime = apcScheduleCallback->callbackTime;
        }
      }
    }
  }
  callbackCheckTime = millis();
}

void ApePixelClock::apcEffectChangeAction(bool reverse)
{
  if (!reverse)
  {
    apcEffectPointer++;
    if (apcEffectPointer >= (int)apcEffects.size())
    {
      apcEffectPointer = 0;
    }
  }
  else
  {
    apcEffectPointer--;
    if (apcEffectPointer < 0)
    {
      apcEffectPointer = (int)apcEffects.size() - 1;
    }
  }
  ApcEffectDef *apcEffect = apcEffects[apcEffectPointer];
  this->apcEffectRefresh(apcEffect);
  this->effectDisplayInit();
}

void ApePixelClock::effectDisplayInit()
{
  ApcEffectDef *apcEffect = apcEffects[apcEffectPointer];
  int areaCount = apcEffect->areaCount;
  for (int i = 0; i < areaCount; i++)
  {
    apcEffect->currentAreaIndex = i;
    this->renderAction(apcEffect);
    this->show();
  }
}

void ApePixelClock::apcEffectRefresh(ApcEffectDef *apcEffect)
{
  apcEffect->currentChangeTime = apcEffect->autoChangeTime;
  for (int i = 0; i < apcEffect->areaCount; i++)
  {
    ApcEffectAreaDef &apcEffectArea = apcEffect->areaDef[i];
    apcEffectArea.currentFrameCount = 0;
    apcEffectArea.currentRefreshTime = 0;
  }
}

void ApePixelClock::show()
{
  APC.plBegin(8).plCallback();
}
void ApePixelClock::clear()
{
  APC.plBegin(9).plCallback();
}

void ApePixelClock::areaClear()
{
  APC.plBegin(23).plCoord(0, 0).plByte(m_areaWidth).plByte(m_areaHeight).plColor(0, 0, 0).plCallback();
}

void ApePixelClock::renderAction(ApcEffectDef *apcEffect, bool needArea)
{
  ApcEffectAreaDef &apcEffectArea = apcEffect->areaDef[apcEffect->currentAreaIndex];
  this->m_offsetX = apcEffectArea.x;
  this->m_offsetY = apcEffectArea.y;
  this->m_areaWidth = apcEffectArea.width;
  this->m_areaHeight = apcEffectArea.height;
  this->areaClear();
  apcEffect_callback(apcEffect->effectId, apcEffect->currentAreaIndex, apcEffectArea.currentFrameCount);
}

void ApePixelClock::publish(String &s)
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.parseObject(s);

  String infoType = json["type"].as<String>();
  if (infoType == "button")
  {
    if (isAlarming && !stopAlarm)
    {
      APC.plBegin(19).plCallback();
      stopAlarm = true;
    }
    if (json["left"] == "short")
    {
      if (myMenue.isMenueMode())
      {
        myMenue.btnPressed(0, 0);
      }
      else
      {
        this->apcEffectChangeAction(true);
      }
    }
    else if (json["left"] == "long")
    {
      myMenue.btnPressed(0, 1);
    }
    else if (json["middle"] == "short")
    {
      myMenue.btnPressed(1, 0);
    }
    else if (json["middle"] == "long")
    {
      myMenue.btnPressed(1, 1);
    }
    else if (json["right"] == "short")
    {
      if (myMenue.isMenueMode())
      {
        myMenue.btnPressed(2, 0);
      }
      else
      {
        this->apcEffectChangeAction(false);
      }
    }
    else if (json["right"] == "long")
    {
      myMenue.btnPressed(2, 1);
    }
  }
  else if (infoType == "MatrixInfo")
  {
    inDoorInfo[0] = json["Temp"].as<int>();
    inDoorInfo[1] = json["Hum"].as<int>();
    inDoorInfo[2] = json["hPa"].as<int>();
    if (json["LUX"])
    {
      ldrInfo = json["LUX"].as<float>();
    }
    else
    {
      ldrInfo = -1;
    }
  }
}

void ApePixelClock::systemInit(MQTT_CALLBACK_SIGNATURE, RTC_DS1307 *rtc, ApcConfigDef *apcConfigDef)
{

  this->callback = callback;
  m_rtc = rtc;
  m_apcConfigDef = apcConfigDef;
  apcEffectPointer = 0;
  this->addApcScheduleCallback(201, 1000);
  this->addApcScheduleCallback(203, 1000);
  this->addApcEffects();
  APC.apcSetup();
}
void ApePixelClock::upgradeTime()
{
  if (this->internetConnected())
  {
    int errCode = 0;
    uint32_t secondStamp = this->requestSecondStamp(&errCode);
    if (errCode == 0)
    {
      m_rtc->adjust(DateTime(secondStamp));
    }
  }
}
bool ApePixelClock::internetConnected()
{
  return WiFi.isConnected();
}

const String url = "http://worldtimeapi.org/api/ip";
uint32_t ApePixelClock::requestSecondStamp(int *errCode)
{
  const String &res = this->httpRequest(url, errCode);
  if (*errCode == 0)
  {
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(res);
    uint32_t secondStamp = json["unixtime"].as<String>().substring(0, 10).toInt();
    int timeZone = json["utc_offset"].as<String>().substring(0, 3).toInt();
    return secondStamp + timeZone * 60 * 60;
  }
  return 0;
}

String ApePixelClock::httpRequest(const String &url, int *errCode)
{
  String res;
  HTTPClient *httpClient = new HTTPClient();
  WiFiClient *wifiClient = new WiFiClient();
  if (httpClient->begin(*wifiClient, url))
  { // HTTP
    int httpCode = httpClient->GET();
    *errCode = httpCode;
    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        *errCode = 0;
        res = httpClient->getString();
      }
    }
    httpClient->end();
  }
  delete httpClient;
  delete wifiClient;
  return res;
}

String ApePixelClock::httpsRequest(const String &url, int *errCode)
{
  String urlTemp = url;
  urlTemp.replace("https://", "");
  int splitIndex = urlTemp.indexOf('/');
  const String &httpsServer = urlTemp.substring(0, splitIndex);
  const String &api = urlTemp.substring(splitIndex);
  Serial.print("Connecting to ");
  Serial.print(httpsServer);
  BearSSL::WiFiClientSecure *httpsClient = new BearSSL::WiFiClientSecure();
  httpsClient->setInsecure();
  httpsClient->setTimeout(2000);
  int retries = 6;
  while (!httpsClient->connect(httpsServer, 443) && (retries-- > 0))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  String res;
  if (!httpsClient->connected())
  {
    Serial.println("Failed to connect, going back to sleep");
    *errCode = -1;
    httpsClient->stop();
  }
  else
  {
    Serial.print("Request resource: ");
    httpsClient->print(String("GET ") + api +
                       " HTTP/1.1\r\n" +
                       "Host: " + httpsServer + "\r\n" +
                       "Connection: close\r\n\r\n");
    int timeout = 5 * 10; // 5 seconds
    while (!httpsClient->available() && (timeout-- > 0))
    {
      delay(100);
    }
    if (!httpsClient->available())
    {
      Serial.println("No response, going back to sleep");
      *errCode = -2;
      httpsClient->stop();
    }
    else
    {
      while (httpsClient->available())
      {
        res += char(httpsClient->read());
      }
      res = res.substring(res.indexOf('{'));
      Serial.println("\nclosing connection");
      httpsClient->stop();
    }
  }
  delete httpsClient;
  return res;
}

ApePixelClock &ApePixelClock::plBegin(byte pid)
{
  payLoadPointer = 0;
  payload[payLoadPointer++] = pid;
  return APC;
}
ApePixelClock &ApePixelClock::plCoord(uint16_t x, uint16_t y, bool offset)
{
  if (offset)
  {
    x += m_offsetX;
    x += m_offsetY;
  }

  payload[payLoadPointer++] = (x >> 8) & 0xFF;
  payload[payLoadPointer++] = (x)&0xFF;
  payload[payLoadPointer++] = (y >> 8) & 0xFF;
  payload[payLoadPointer++] = (y)&0xFF;
  return APC;
}
ApePixelClock &ApePixelClock::plByte(byte b)
{
  payload[payLoadPointer++] = b;
  return APC;
}
ApePixelClock &ApePixelClock::plColor(byte r, byte g, byte b)
{
  payload[payLoadPointer++] = r;
  payload[payLoadPointer++] = g;
  payload[payLoadPointer++] = b;
  return APC;
}
ApePixelClock &ApePixelClock::plStr(const String &str)
{
  int length = str.length();
  for (int i = 0; i < length; i++)
  {
    payload[payLoadPointer++] = str[i];
  }
  return APC;
}

void ApePixelClock::plCallback()
{
  APC.callback(nullptr, payload, payLoadPointer);
}

int ApePixelClock::textCenterX(int strLength, int charWidth, int maxCharCount)
{
  if (strLength > maxCharCount)
    strLength = maxCharCount;
  // if (strLength > maxCharCount){
  //   for (int x = 0; x <= maxCharCount; x++){

  //     strLength = x;
  //     // maxCharCount
  //   }
  // }
  return (maxCharCount - strLength) * charWidth / 2 ;
  // int center;
  // for (int i = 0; i < 5; i++){
  // center = (maxCharCount - strLength) * charWidth / 2 - i;
  // delay(60);
  // }
  // return center;
}

String ApePixelClock::subscriberCountFormat(uint32 subscriberCount)   //订阅者计数格式
{
  String res;
  if (subscriberCount >= 10000000)
  {
    char numChar[8];
    sprintf(numChar, "%.1f", subscriberCount / 1000000.0);
    res += String(numChar) + "M";
  }
  else if (subscriberCount >= 1000000)
  {
    char numChar[6];
    sprintf(numChar, "%.2f", subscriberCount / 1000000.0);
    res += String(numChar) + "M";
  }
  else if (subscriberCount >= 100000)
  {
    char numChar[8];
    sprintf(numChar, "%.1f", subscriberCount / 1000.0);
    res += String(numChar) + "K";
  }
  else if (subscriberCount >= 10000)
  {
    char numChar[6];
    sprintf(numChar, "%.2f", subscriberCount / 1000.0);
    res += String(numChar) + "K";
  }
  else
  {
    res += String(subscriberCount);
  }
  return res;
}

void ApePixelClock::drawColorIndexFrame(const uint32 *colorMap,
                                        unsigned char width, unsigned char height, const uint32 *pixels)
{
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int count = y * (((width - 1) / 4) + 1) + (x / 4);
      unsigned long color = colorMap[pixels[count] >> (((3 - x % 4)) * 8) & 0xFF];
      APC.plBegin(4).plCoord(x, y).plColor(color >> 16 & 0xFF, color >> 8 & 0xFF, color & 0xFF).plCallback();
    }
  }
}

ApePixelClock APC;