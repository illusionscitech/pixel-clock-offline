#ifndef __APEPIXELCLOCK_H
#define __APEPIXELCLOCK_H

#include <Arduino.h>
#include "RTClib.h"
#include <PubSubClient.h>

////////////////////////////////配置区域--开始////////////////////////////////////
#define GLOBAL_TEXT_COLOR   0xFFFFFF      //默认文字颜色
#define TIME_SHOW           1             //时间效果
#define WEATHER_SHOW        1             //实时天气
#define BILIBILI_SHOW       1             //B站订阅人数
#define YOUTUBE_SHOW        1             //Youtube订阅人数
#define INDOOR_SHOW         1             //室内温湿度
#define COUNTDOWN_DAY_SHOW  0             //日期倒计时
#define RAIN_SHOW           1             //雨落效果
#define TETRIS_SHOW           1             //俄罗斯方块效果

#define BILIBILI_UID      "107475001"                                 //B站用户ID
#define YOUTUBE_CHANNEL   "UCJtwdm4TFqzIRe3fBRl1xJQ"                  //Youtube 频道ID
#define YOUTUBE_APIKEY    "AIzaSyAjdLag8T-AFaub4hEZF4X1BqxLYMm3jBA"   //Youtube APIKEY
#define WEATHER_APIKEY    "d800a016a0bf4289bc62bf5797d64437"          //天气 APIKEY
#define WEATHER_CITY      "101090307"                                 //天气 城市

////////////////////////////////配置区域--结束////////////////////////////////////

#define MAX_APCEffECTAREA_COUNT 4

struct ApcEffectAreaDef
{
  int16_t x;                  // from top-left to rigth-bottom
  int16_t y;
  uint8   width;
  uint8   height;
  uint8   frameCount;
  uint32  frameRefreshTime;   //ms
  uint8   currentFrameCount;
  uint32  currentRefreshTime;
};

typedef void (*ApcScheduleCallback)();
typedef void (*ApcEffectCallback)(unsigned int, unsigned int);

struct ApcEffectDef
{
  uint8 areaCount;
  ApcEffectAreaDef areaDef[MAX_APCEffECTAREA_COUNT];
  uint8 effectId;
  uint8 currentAreaIndex;
  uint32 updateTime;
  uint32 autoChangeTime;
  uint32 currentChangeTime;
};

struct ApcScheduleCallbackDef
{
  uint8 callbackId;
  uint32 callbackTime;
  uint32 currentRefreshTime;
  ApcScheduleCallback callbackFunc;
};

struct ApcConfigDef
{
  bool alarm_enable;
  char alarm_time[6];
  char cdd_date[11];
  int  volume;
  int  brightness;
  bool liteMode;
  bool offLine;
};

class ApePixelClock
{
public:
  MQTT_CALLBACK_SIGNATURE;
private:
  uint16_t m_offsetX;
  uint16_t m_offsetY;
  byte m_areaWidth;
  byte m_areaHeight;
public:
  ApePixelClock();
  void systemInit(MQTT_CALLBACK_SIGNATURE, RTC_DS1307 *rtc,ApcConfigDef *apcConfigDef);
  void apcSetup();
  void apcLoop();
  void publish(String &s);
  String httpRequest(const String& url, int* errCode);
  String httpsRequest(const String& url, int* errCode);
  ApePixelClock &plBegin(byte pid);
  ApePixelClock &plCoord(uint16_t x,uint16_t y,bool offset = true);
  ApePixelClock &plByte(byte b);
  ApePixelClock &plColor(byte r = (GLOBAL_TEXT_COLOR >> 16) & 0xFF,
                         byte g = (GLOBAL_TEXT_COLOR >> 8) & 0xFF,
                         byte b = (GLOBAL_TEXT_COLOR >> 0) & 0xFF);
  ApePixelClock &plStr(const String &str);
  void plCallback();
  int textCenterX(int strLength,int charWidth,int maxCharCount);
  String subscriberCountFormat(uint32 subscriberCount);
  void drawColorIndexFrame(const uint32* colorMap,
  unsigned char width, unsigned char height, const uint32* pixels);
  void ramCheck(const char*);
  void checkAlarm();
  void checkLDR();
  void removeApcScheduleCallback(ApcScheduleCallbackDef* callbackDef);
  ApcConfigDef* myApcConfigDef();
private:
  bool internetConnected();
  void upgradeTime();
  uint32_t requestSecondStamp(int *errCode);
  void effectDisplayInit();
  void apcEffectChangeAction(bool reverse = false);
  void apcEffectRefresh(ApcEffectDef *apcEffect);
  void addApcEffects();
  void addApcEffect(ApcEffectDef *apcEffect);
  ApcScheduleCallbackDef* addApcScheduleCallback(int callbackId,unsigned long callbackTime,bool callbackNow = true);
  
  void renderCheck();
  void apcCallbackAction();
  void renderAction(ApcEffectDef *apcEffect, bool needArea = true);
  void show();
  void clear();
  void areaClear();
  
};

extern ApePixelClock APC;

#endif // __APEPIXELCLOCK_H
