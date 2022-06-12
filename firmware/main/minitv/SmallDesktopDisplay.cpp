/* *****************************************************************
 * 
 * SmallDesktopDisplay
 *    小型桌面显示器
 * 
 * 原  作  者：Misaka
 * 修      改：微车游
 * 讨  论  群：811058758、887171863
 * 创 建 日 期：2021.07.19
 * 最后更改日期：2021.09.18
 * 更 改 说 明：V1.1添加串口调试，波特率115200\8\n\1；增加版本号显示。
 *            V1.2亮度和城市代码保存到EEPROM，断电可保存
 *            V1.3.1 更改smartconfig改为WEB配网模式，同时在配网的同时增加亮度、屏幕方向设置。
 *            V1.3.2 增加wifi休眠模式，仅在需要连接的情况下开启wifi，其他时间关闭wifi。增加wifi保存至eeprom（目前仅保存一组ssid和密码）
 *            V1.3.3  修改WiFi保存后无法删除的问题。目前更改为使用串口控制，输入 0x05 重置WiFi数据并重启。
 *                    增加web配网以及串口设置天气更新时间的功能。
 *            V1.3.4  修改web配网页面设置，将wifi设置页面以及其余设置选项放入同一页面中。
 *                    增加web页面设置是否使用DHT传感器。（使能DHT后才可使用）
 *            V1.4    增加web服务器，使用web网页进行设置。由于使用了web服务器，无法开启WiFi休眠。
 *                    注意，此版本中的DHT11传感器和太空人图片选择可以通过web网页设置来进行选择，无需通过使能标志来重新编译。
 * 
 * 引 脚 分 配： SCK  GPIO14
 *             MOSI  GPIO13
 *             RES   GPIO2
 *             DC    GPIO0
 *             LCDBL GPIO5
 *             
 *             增加DHT11温湿度传感器，传感器接口为 GPIO 12
 * 
 *    感谢群友 @你别失望  提醒发现WiFi保存后无法重置的问题，目前已解决。详情查看更改说明！
 * *****************************************************************/
#define Version  "SDD V1.4"
/* *****************************************************************
 *  库文件、头文件
 * *****************************************************************/


/* *****************************************************************
 *  配置使能位
 * *****************************************************************/
//WEB配网使能标志位----WEB配网打开后会默认关闭smartconfig功能
#define WM_EN   1


#if WM_EN
#include <WiFiManager.h>
//WiFiManager 参数
WiFiManager wm; // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#endif








/* *****************************************************************
 *  参数设置
 * *****************************************************************/

struct config_type
{
  char stassid[32];//定义配网得到的WIFI名长度(最大32字节)
  char stapsw[64];//定义配网得到的WIFI密码长度(最大64字节)
};

//---------------修改此处""内的信息--------------------
//如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf ={{""},{""}};






//函数声明
time_t getNtpTime();
void digitalClockDisplay(int reflash_en);
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress &address);
void LCD_reflash(int en);
void savewificonfig();
void readwificonfig();
void deletewificonfig();
#if WebSever_EN
void Web_Sever_Init();
void Web_Sever();
#endif
void saveCityCodetoEEP(int * citycode);
void readCityCodefromEEP(int * citycode);



#if WebSever_EN
//web网站相关函数
//web设置页面
void handleconfig()
{
  String msg;
  int web_cc,web_setro,web_lcdbl,web_upt,web_dhten;

  if (server.hasArg("web_ccode") || server.hasArg("web_bl") || \
      server.hasArg("web_upwe_t") || server.hasArg("web_DHT11_en") || server.hasArg("web_set_rotation")) 
  {
    web_cc    = server.arg("web_ccode").toInt();
    web_setro = server.arg("web_set_rotation").toInt();
    web_lcdbl = server.arg("web_bl").toInt();
    web_upt   = server.arg("web_upwe_t").toInt();
    web_dhten = server.arg("web_DHT11_en").toInt();
    Serial.println("");
    if(web_cc>=101000000 && web_cc<=102000000) 
    {
      saveCityCodetoEEP(&web_cc);
      readCityCodefromEEP(&web_cc);
      cityCode = web_cc;
      Serial.print("城市代码:");
      Serial.println(web_cc);
    }
    if(web_lcdbl>0 && web_lcdbl<=100)
    {
      EEPROM.write(BL_addr, web_lcdbl);//亮度地址写入亮度值
      EEPROM.commit();//保存更改的数据
      delay(5);
      LCD_BL_PWM = EEPROM.read(BL_addr); 
      delay(5);
      Serial.printf("亮度调整为：");
      analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM*10));
      Serial.println(LCD_BL_PWM);
      Serial.println("");
    }
    if(web_upt > 0 && web_upt <= 60)
    {
      EEPROM.write(UpWeT_addr, web_upt);//亮度地址写入亮度值
      EEPROM.commit();//保存更改的数据
      delay(5);
      updateweater_time = web_upt;
      Serial.print("天气更新时间（分钟）:");
      Serial.println(web_upt);
    }

    EEPROM.write(DHT_addr, web_dhten);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_dhten != DHT_img_flag)
    {
      DHT_img_flag = web_dhten;
      tft.fillScreen(0x0000);
      LCD_reflash(1);//屏幕刷新程序
      UpdateWeater_en = 1;
      TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
      TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标
    }
    Serial.print("DHT Sensor Enable： ");
    Serial.println(DHT_img_flag);

    
    EEPROM.write(Ro_addr, web_setro);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_setro != LCD_Rotation)
    {
      LCD_Rotation = web_setro;
      tft.setRotation(LCD_Rotation);
      tft.fillScreen(0x0000);
      LCD_reflash(1);//屏幕刷新程序
      UpdateWeater_en = 1;
      TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
      TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标
    }
    Serial.print("LCD Rotation:");
    Serial.println(LCD_Rotation);
  }

  //网页界面代码段
  String content = "<html><style>html,body{ background: #1aceff; color: #fff; font-size: 10px;}</style>";
        content += "<body><form action='/' method='POST'><br><div>SDD Web Config</div><br>";
        content += "City Code:<br><input type='text' name='web_ccode' placeholder='city code'><br>";
        content += "<br>Back Light(1-100):(default:50)<br><input type='text' name='web_bl' placeholder='10'><br>";
        content += "<br>Weather Update Time:(default:10)<br><input type='text' name='web_upwe_t' placeholder='10'><br>";
        #if DHT_EN
        content += "<br>DHT Sensor Enable  <input type='radio' name='web_DHT11_en' value='0'checked> DIS \
                                          <input type='radio' name='web_DHT11_en' value='1'> EN<br>";
        #endif
        content += "<br>LCD Rotation<br>\
                    <input type='radio' name='web_set_rotation' value='0' checked> USB Down<br>\
                    <input type='radio' name='web_set_rotation' value='1'> USB Right<br>\
                    <input type='radio' name='web_set_rotation' value='2'> USB Up<br>\
                    <input type='radio' name='web_set_rotation' value='3'> USB Left<br>";
        content += "<br><div><input type='submit' name='Save' value='Save'></form></div>" + msg + "<br>";
        content += "By WCY<br>";
        content += "</body></html>";
  server.send(200, "text/html", content);
}

//no need authentication
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

//Web服务初始化
void Web_Sever_Init()
{
  uint32_t counttime = 0;//记录创建mDNS的时间
  Serial.println("mDNS responder building...");
  counttime = millis();
  while (!MDNS.begin("SD3"))
  {
    if(millis() - counttime > 30000) ESP.restart();//判断超过30秒钟就重启设备
  }

  Serial.println("mDNS responder started");
  //输出连接wifi后的IP地址
  // Serial.print("本地IP： ");
  // Serial.println(WiFi.localIP());

  server.on("/", handleconfig);
  server.onNotFound(handleNotFound);

  //开启TCP服务
  server.begin();
  Serial.println("HTTP服务器已开启");

  Serial.println("连接: http://sd3.local");
  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  //将服务器添加到mDNS
  MDNS.addService("http", "tcp", 80);
}
//Web网页设置函数
void Web_Sever()
{
  MDNS.update();
  server.handleClient();
}
//web服务打开后LCD显示登陆网址及IP
void Web_sever_Win()
{
  IPAddress IP_adr = WiFi.localIP();
  // strcpy(IP_adr,WiFi.localIP().toString());
  clk.setColorDepth(8);
  
  clk.createSprite(200, 70);//创建窗口
  clk.fillSprite(0x0000);   //填充率

  // clk.drawRoundRect(0,0,200,100,5,0xFFFF);       //空心圆角矩形
  clk.setTextDatum(CC_DATUM);   //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000); 
  clk.drawString("Connect to Config:",70,10,2);
  // clk.drawString("IP:",45,60,2);
  clk.setTextColor(TFT_WHITE, 0x0000); 
  clk.drawString("http://sd3.local",100,40,4);
  // clk.drawString(&IP_adr,125,70,2);
  clk.pushSprite(20,40);  //窗口位置
    
  clk.deleteSprite();
}
#endif

#if WM_EN

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  
  Serial.println("[CALLBACK] saveParamCallback fired");
  // Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  // Serial.println("PARAM CityCode = " + getParam("CityCode"));
  // Serial.println("PARAM LCD BackLight = " + getParam("LCDBL"));
  // Serial.println("PARAM WeaterUpdateTime = " + getParam("WeaterUpdateTime"));
  // Serial.println("PARAM Rotation = " + getParam("set_rotation"));
  // Serial.println("PARAM DHT11_en = " + getParam("DHT11_en"));
  
 
}

//WEB配网函数
void Webconfig()
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  
  delay(3000);
  wm.resetSettings(); // wipe settings
  
  // add a custom input field
  int customFieldLength = 40;
  
  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");
  
  // test custom html input type(checkbox)
  //  new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type
  
  // test custom html(radio)
  // const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  // new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

  const char* set_rotation = "<br/><label for='set_rotation'>Set Rotation</label>\
                              <input type='radio' name='set_rotation' value='0' checked> One<br>\
                              <input type='radio' name='set_rotation' value='1'> Two<br>\
                              <input type='radio' name='set_rotation' value='2'> Three<br>\
                              <input type='radio' name='set_rotation' value='3'> Four<br>";
  WiFiManagerParameter  custom_rot(set_rotation); // custom html input
  WiFiManagerParameter  custom_bl("LCDBL","LCD BackLight(1-100)","10",3);
  #if DHT_EN
  WiFiManagerParameter  custom_DHT11_en("DHT11_en","Enable DHT11 sensor","0",1);
  #endif
  WiFiManagerParameter  custom_weatertime("WeaterUpdateTime","Weather Update Time(Min)","10",3);
  WiFiManagerParameter  custom_cc("CityCode","CityCode","101250101",9);
  WiFiManagerParameter  p_lineBreak_notext("<p></p>");

  // wm.addParameter(&p_lineBreak_notext);
  // wm.addParameter(&custom_field);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_cc);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_bl);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_weatertime);
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_rot);
  #if DHT_EN
  wm.addParameter(&p_lineBreak_notext);
  wm.addParameter(&custom_DHT11_en);
  #endif
  wm.setSaveParamsCallback(saveParamCallback);
  
  // custom menu via array or vector
  // 
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi","restart"};
  wm.setMenu(menu);
  
  // set dark theme
  wm.setClass("invert");
  
  //set static ip
  // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  // wm.setShowStaticFields(true); // force show static ip fields
  // wm.setShowDnsFields(true);    // force show dns field always

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
//  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons
  
  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
   res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  //  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
  
  while(!res);
}


#endif





extern "C" void app_main()
{
  initArduino();

  Serial.begin(115200);
  // EEPROM.begin(1024);
  // WiFi.forceSleepWake();
  // wm.resetSettings();    //在初始化中使wifi重置，需重新配置WiFi
  
int loadNum=0;
  // WiFi.begin("ASUS", "asfsdgfdfhdf");
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    loadNum++;
    delay(10); 
    ESP_LOGI("TAG", "wifi config ");
      
    if(loadNum>=194)
    {
      Webconfig();
      break;
    }
  }
  delay(10); 

  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.print("SSID:");
    Serial.println(WiFi.SSID().c_str());
    Serial.print("PSW:");
    Serial.println(WiFi.psk().c_str());
   
  }

}

