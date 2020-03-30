/*
  Draws a 3d rotating cube on the freetronics OLED screen.
  Original code was found at http://forum.freetronics.com/viewtopic.php?f=37&t=5495
  Thanks to Adafruit at http://www.adafruit.com for the great display and sensor libraries
*/
#include <M5Stack.h>
#include <M5StackUpdater.h>
#include <WiFi.h>
#include <Wire.h>

#define BUTTON_PAUSE_PIN BUTTON_B_PIN               //暂停按钮IO口
#define BUTTON_PLUS_PIN BUTTON_A_PIN                //Z轴偏移量增加
#define BUTTON_MINUS_PIN BUTTON_C_PIN               //Z轴偏移量减小

///////注：下面的长度为相对长度，不是屏幕上显示的像素点数///////
#define AXIS_LENGTH 320                   //坐标轴长度/2
#define ARROW_LENGTH 50                   //坐标轴箭头长度

#define CUBE_SIZE 50                      //正方体大小/2
#define MAX_CUBES 100                     //最多100个正方体,再多可能报错，如果需要更多可以在psram中申请内存
const int MPU_ADDRESS = 0x68;             // I2C address of the MPU-6050

int16_t AcX, AcY, AcX_Cali, AcY_Cali;
//For cube rotation
int16_t xOut = 0;
int16_t yOut = 0;

float xx, xy, xz;
float yx, yy, yz;
float zx, zy, zz;

float fact;

int16_t Xan, Yan;                       //x、y轴角度

int16_t Xoff;
int16_t Yoff;
int16_t Zoff;

struct Point3d
{
  int16_t x;
  int16_t y;
  int16_t z;
};

struct Point2d
{
  int16_t x;
  int16_t y;
};



uint32_t LinestoRender; // lines to render.
uint32_t OldLinestoRender; // lines to render just in case it changes. this makes sure the old lines all get erased.

struct Line3d
{
  Point3d p0;
  Point3d p1;
};

struct Line2d
{
  Point2d p0;
  Point2d p1;
};
Line3d Lines[12 * MAX_CUBES + 9]; //Number of lines to render
Line2d Render[12 * MAX_CUBES + 9];
Line2d ORender[12 * MAX_CUBES + 9];

/***********************************************************************************************************************************/
// Sets the global vars for the 3d transform. Any points sent through "process" will be transformed using these figures.
// only needs to be called if Xan or Yan are changed.
void SetVars(void)
{
  float Xan2, Yan2, Zan2;
  float s1, s2, s3, c1, c2, c3;

  Xan2 = Xan / fact; // convert degrees to radians.
  Yan2 = Yan / fact;

  // Zan is assumed to be zero

  s1 = sin(Yan2);
  s2 = sin(Xan2);

  c1 = cos(Yan2);
  c2 = cos(Xan2);

  xx = c1;
  xy = 0;
  xz = -s1;

  yx = (s1 * s2);
  yy = c2;
  yz = (c1 * s2);

  zx = (s1 * c2);
  zy = -s2;
  zz = (c1 * c2);
}


/***********************************************************************************************************************************/
// processes x1,y1,z1 and returns rx1,ry1 transformed by the variables set in SetVars()
// fairly heavy on floating point here.
// uses a bunch of global vars. Could be rewritten with a struct but not worth the effort.
void ProcessLine(struct Line2d *ret, struct Line3d vec)
{
  float zvt1;
  int16_t xv1, yv1, zv1;

  float zvt2;
  int16_t xv2, yv2, zv2;

  int16_t rx1, ry1;
  int16_t rx2, ry2;

  int16_t x1;
  int16_t y1;
  int16_t z1;

  int16_t x2;
  int16_t y2;
  int16_t z2;

  int16_t Ok;

  x1 = vec.p0.x;
  y1 = vec.p0.y;
  z1 = vec.p0.z;

  x2 = vec.p1.x;
  y2 = vec.p1.y;
  z2 = vec.p1.z;

  Ok = 0; // defaults to not OK

  xv1 = (x1 * xx) + (y1 * xy) + (z1 * xz);
  yv1 = (x1 * yx) + (y1 * yy) + (z1 * yz);
  zv1 = (x1 * zx) + (y1 * zy) + (z1 * zz);

  zvt1 = zv1 - Zoff;


  if ( zvt1 < -5) {
    rx1 = 256 * (xv1 / zvt1) + Xoff;
    ry1 = 256 * (yv1 / zvt1) + Yoff;
    Ok = 1; // ok we are alright for point 1.
  }


  xv2 = (x2 * xx) + (y2 * xy) + (z2 * xz);
  yv2 = (x2 * yx) + (y2 * yy) + (z2 * yz);
  zv2 = (x2 * zx) + (y2 * zy) + (z2 * zz);

  zvt2 = zv2 - Zoff;


  if ( zvt2 < -5) {
    rx2 = 256 * (xv2 / zvt2) + Xoff;
    ry2 = 256 * (yv2 / zvt2) + Yoff;
  } else
  {
    Ok = 0;
  }

  if (Ok == 1) {
    ret->p0.x = rx1;
    ret->p0.y = ry1;

    ret->p1.x = rx2;
    ret->p1.y = ry2;
  }
  // The ifs here are checks for out of bounds. needs a bit more code here to "safe" lines that will be way out of whack, so they dont get drawn and cause screen garbage.

}

//添加立方体
int8_t CubeTable[] = {
  //前面
  -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE,
  CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE,
  CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE,
  -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE,
  //后面
  -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
  CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE,
  CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE,
  -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
  //侧面
  -CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
  CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE,
  -CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE,
  CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, -CUBE_SIZE
};

void AddCube(int8_t x, int8_t y, int8_t z)
{
  uint8_t j = 0;
  for (uint8_t i = 0; i < 12; ++i)
  {
    Lines[LinestoRender].p0.x = CubeTable[j++] + CUBE_SIZE * x * 2;
    Lines[LinestoRender].p0.y = CubeTable[j++] + CUBE_SIZE * y * 2;
    Lines[LinestoRender].p0.z = CubeTable[j++] + CUBE_SIZE * z * 2;
    Lines[LinestoRender].p1.x = CubeTable[j++] + CUBE_SIZE * x * 2;
    Lines[LinestoRender].p1.y = CubeTable[j++] + CUBE_SIZE * y * 2;
    Lines[LinestoRender].p1.z = CubeTable[j++] + CUBE_SIZE * z * 2;
    LinestoRender++;
  }
  OldLinestoRender = LinestoRender;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//坐标轴绘制

int32_t AxisTable[] = {
  -AXIS_LENGTH, 0, 0, AXIS_LENGTH, 0, 0,      //主视方向，红色
  AXIS_LENGTH , 0, 0, AXIS_LENGTH - ARROW_LENGTH, ARROW_LENGTH, 0,
  AXIS_LENGTH, 0, 0, AXIS_LENGTH - ARROW_LENGTH, -ARROW_LENGTH, 0,

  0, -AXIS_LENGTH, 0, 0, AXIS_LENGTH, 0,      //俯视方向，绿色
  0, -AXIS_LENGTH, 0, 0, -AXIS_LENGTH + ARROW_LENGTH, ARROW_LENGTH,
  0, -AXIS_LENGTH, 0, 0, -AXIS_LENGTH + ARROW_LENGTH, -ARROW_LENGTH,

  0, 0, -AXIS_LENGTH, 0, 0, AXIS_LENGTH,      //左视方向，蓝色
  0, 0, -AXIS_LENGTH, ARROW_LENGTH, 0, -AXIS_LENGTH + ARROW_LENGTH,
  0, 0, -AXIS_LENGTH, -ARROW_LENGTH, 0, -AXIS_LENGTH + ARROW_LENGTH
};
void draw3DAxis()
{
  LinestoRender = 0;           //线段条数初始化
  uint8_t j = 0;
  for (uint8_t i = 0; i < 9; ++i)
  {
    Lines[LinestoRender].p0.x = AxisTable[j++];
    Lines[LinestoRender].p0.y = AxisTable[j++];
    Lines[LinestoRender].p0.z = AxisTable[j++];
    Lines[LinestoRender].p1.x = AxisTable[j++];
    Lines[LinestoRender].p1.y = AxisTable[j++];
    Lines[LinestoRender].p1.z = AxisTable[j++];
    LinestoRender++;
  }
}
/***********************************************************************************************************************************/
void setup() {
  //for M5Stack_SDUpdater
  M5.begin();
  Wire.begin();
  if (digitalRead(BUTTON_A_PIN) == 0) {
    //Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }

  //关闭WIFI
  WiFi.mode(WIFI_OFF);
  M5.Lcd.fillScreen(TFT_BLACK);   // clears the screen and buffer
  M5.Lcd.setTextSize(2);

  draw3DAxis();
  ChangeView();

  M5.Lcd.print("      IMU Calibration\nPut the machine on a flat table");
  fact = 180 / 3.14159265358979323846264338327950; // conversion from degrees to radians.

  Xoff = 160; // x轴原点，通常为屏幕中心（但是这里原来是是90）
  Yoff = 120; // 通常为屏幕尺寸/2，y轴原点
  Zoff = 1400;   //方块尺寸（z轴偏移量），较大的数-》较小的方块
  //注意：数值过小会导致渲染过慢


  // Initialize MPU_ADDRESS
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU_ADDRESS-6050)
  Wire.endTransmission(true);
  delay(1000);

  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(true);
  Wire.requestFrom(MPU_ADDRESS, 4, true); // request a total of 14 registers
  AcX_Cali = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY_Cali = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)

  M5.Lcd.fillScreen(TFT_BLACK);   // clears the screen and buffer
}
/***********************************************************************************************************************************/
void RenderImage( void)                         //绘图函数
{
  // renders all the lines after erasing the old ones.
  // in here is the only code actually interfacing with the OLED. so if you use a different lib, this is where to change it.
  //在这里修改屏幕类型
  //使用的M5.Lcd

  for (uint32_t i = 0; i < 9; i++ )
  {
    M5.Lcd.drawLine(ORender[i].p0.x, ORender[i].p0.y, ORender[i].p1.x, ORender[i].p1.y, TFT_BLACK); // erase the old lines.
  }

  for (uint8_t i = 0; i < 3; ++i)
  {
    M5.Lcd.drawLine(Render[i].p0.x, Render[i].p0.y, Render[i].p1.x, Render[i].p1.y, TFT_RED);
  }
  for (uint8_t i = 3; i < 6; ++i)
  {
    M5.Lcd.drawLine(Render[i].p0.x, Render[i].p0.y, Render[i].p1.x, Render[i].p1.y, TFT_GREEN);
  }
  for (uint8_t i = 6; i < 9; ++i)
  {
    M5.Lcd.drawLine(Render[i].p0.x, Render[i].p0.y, Render[i].p1.x, Render[i].p1.y, TFT_BLUE);
  }

  if (OldLinestoRender == LinestoRender)
  {
    //下面是绘制部分
    for (uint32_t i = 9; i < LinestoRender; i++ )
    {
      M5.Lcd.drawLine(ORender[i].p0.x, ORender[i].p0.y, ORender[i].p1.x, ORender[i].p1.y, TFT_BLACK); // erase the old lines.
      M5.Lcd.drawLine(Render[i].p0.x, Render[i].p0.y, Render[i].p1.x, Render[i].p1.y, TFT_WHITE);
    }
  }
  else
  {
    for (uint32_t i = 9; i < OldLinestoRender; i++ )
    {
      M5.Lcd.drawLine(ORender[i].p0.x, ORender[i].p0.y, ORender[i].p1.x, ORender[i].p1.y, TFT_BLACK); // erase the old lines.
    }
    for (uint32_t i = 9; i < LinestoRender; i++ )
    {
      M5.Lcd.drawLine(Render[i].p0.x, Render[i].p0.y, Render[i].p1.x, Render[i].p1.y, TFT_WHITE);
    }
    OldLinestoRender = LinestoRender;
  }

  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(true);
  Wire.requestFrom(MPU_ADDRESS, 4, true); // request a total of 14 registers
  AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  //下面的信息不显示了
  /*
    // text display tests
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(0,0);
    //Display ACC
    M5.Lcd.print("AcX: ");
    M5.Lcd.println(AcX);
    M5.Lcd.print("AcY: ");
    M5.Lcd.println(AcY);
  */

}


/***********************************************************************************************************************************/


void loop() {

  while (!digitalRead(BUTTON_PAUSE_PIN));
  if (!digitalRead(BUTTON_PLUS_PIN))
  {
    Zoff += 10;
    if (Zoff > 8000)
      Zoff = 8000;
    if (!digitalRead(BUTTON_MINUS_PIN))
    {
      ESP.restart();
    }
  }
  if (!digitalRead(BUTTON_MINUS_PIN))
  {
    Zoff -= 10;
    if (Zoff < 30)
      Zoff = 30;
  }
  //根据自己的imu安装方式修改，我的imu的x轴和y轴是反的，所以这样改
  yOut = map(AcX - AcX_Cali, -17000, 17000, -5, 5);
  xOut = map(AcY - AcY_Cali, -17000, 17000, -5, 5);

  //根据自己的习惯的旋转方式修改下面两行
  Xan += xOut;
  Yan += -yOut;

  Yan = Yan % 360;
  Xan = Xan % 360; // prevents overflow.


  SetVars(); //sets up the global vars to do the conversion.

  for (uint32_t i = 0; i < LinestoRender ; i++)
  {
    ORender[i] = Render[i]; // stores the old line segment so we can delete it later.
    ProcessLine(&Render[i], Lines[i]); // converts the 3d line segments to 2d.
  }

  RenderImage(); // go draw it!
  if (Zoff > 100)
  {
    delay(10);
  }
}
