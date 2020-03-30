#define VIEW_MAIN 0
#define VIEW_LEFT 1
#define VIEW_TOP 2

uint8_t viewTable_Row[3][5];       //每行有几个正方形，由CountRow返回
uint8_t viewTable_Column[3][5];       //每列有几个正方形，由CountRow返回
uint8_t viewTable[3][5][5];             //三视图，1为这个点有正方形，0为这个点没有正方形

uint16_t MaxCube = 0;
uint16_t MinCube = 0;
uint8_t Cube3D_Data[5][5];            //3D视图，为n则这个位置有n个方块，为0则没有

#define LONGPRESS_TIME 500

uint8_t max(uint8_t num1, uint8_t num2)
{
  if (num1 > num2)
    return num1;
  return num2;
}
/*
  void CountRow(uint8_t table[5][5], uint8_t Which)                        //计算行内有几个
  {
  for (uint8_t i = 0; i < 5; ++i)
  {
    for (uint8_t j = 0; j < 5; ++j)
    {
      viewTable_Row[Which][i] += table[i][j];
    }
  }
  }
*/
void CountColumn(uint8_t table[5][5], uint8_t Which)
{
  for (uint8_t i = 0; i < 5; ++i)
  {
    for (uint8_t j = 0; j < 5; ++j)
    {
      viewTable_Column[Which][i] += table[i][j];
    }
  }
}

void Calculate3V()                                    //已知完整3视图
{
  CountColumn(viewTable[VIEW_MAIN], VIEW_MAIN);
  CountColumn(viewTable[VIEW_LEFT], VIEW_LEFT);
  for (uint8_t i = 0; i < 5; ++i)                     //选择行
  {
    for (uint8_t j = 0; j < 5; ++j)                   //选择列
    {
      if (viewTable[VIEW_TOP][i][j] == 1)
      {
        Cube3D_Data[i][j] = min(viewTable_Column[VIEW_MAIN][j], viewTable_Column[VIEW_LEFT][i]);
        MaxCube = MinCube = MinCube + Cube3D_Data[i][j];
      }
    }
  }
}

void CalculateML()                                    //已知主视图、左视图
{
  uint8_t lineFlag = 0;
  CountColumn(viewTable[VIEW_MAIN], VIEW_MAIN);
  CountColumn(viewTable[VIEW_LEFT], VIEW_LEFT);
  for (uint8_t i = 0; i < 5; ++i)                     //选择行
  {
    lineFlag = 0;
    for (uint8_t j = 0; j < 5; ++j)                   //选择列
    {
      if (viewTable_Column[VIEW_MAIN][j] == viewTable_Column[VIEW_LEFT][i])
      {
        Cube3D_Data[i][j] = viewTable_Column[VIEW_MAIN][j];
        MinCube = MinCube + Cube3D_Data[i][j];
        lineFlag = 1;
      }
      MaxCube += min(viewTable_Column[VIEW_MAIN][j], viewTable_Column[VIEW_LEFT][i]);
    }
  }
}

void CalculateLT()                                    //已知左视图、俯视图
{
  uint8_t MaxAt = 0;                                  //在最后显示最高的方块
  CountColumn(viewTable[VIEW_LEFT], VIEW_LEFT);
  for (uint8_t i = 0; i < 5; ++i)                     //选择行
  {
    for (uint8_t j = 0; j < 5; ++j)                   //找到当前行最后一个
    {
      if (viewTable[VIEW_TOP][i][j] == 1)
      {
        MaxAt = j;
      }
    }
    for (uint8_t j = 0; j < 5; ++j)                   //选择列
    {
      if (viewTable[VIEW_TOP][i][j] == 1)
      {
        if (j == MaxAt)
        {
          Cube3D_Data[i][j] =viewTable_Column[VIEW_LEFT][i];
        }
        else
        {
          Cube3D_Data[i][j] = 1;
        }
        //Cube3D_Data[i][j] = min(1, viewTable_Column[VIEW_LEFT][j]);
        MinCube = MinCube + Cube3D_Data[i][j];
        MaxCube = MaxCube + viewTable_Column[VIEW_LEFT][i];
      }
    }
  }
}

void CalculateMT()                                    //已知主视图、俯视图
{
  uint8_t MaxAt = 0;
  CountColumn(viewTable[VIEW_MAIN], VIEW_MAIN);
  for (uint8_t i = 0; i < 5; ++i)                     //选择行
  {
    for (uint8_t j = 0; j < 5; ++j)                   //测试当前行最后一个
    {
      if (viewTable[VIEW_TOP][i][j] == 1)
      {
        MaxAt = j;
      }
    }
    for (uint8_t j = 0; j < 5; ++j)                   //选择列
    {
      if (viewTable[VIEW_TOP][i][j] == 1)
      {
        if (j == MaxAt)
        {
          Cube3D_Data[i][j] =viewTable_Column[VIEW_MAIN][i];
        }
        else
        {
          Cube3D_Data[i][j] =1;
        }
        MinCube = MinCube + Cube3D_Data[i][j];
        MaxCube = MaxCube + viewTable_Column[VIEW_MAIN][j];
      }
    }
  }
}

void Draw3DCube()
{
  for (uint8_t i = 0; i < 5; ++i)
  {
    for (uint8_t j = 0; j < 5; ++j)
    {
      if (Cube3D_Data[i][j] != 0)
      {
        uint8_t CubeCount = Cube3D_Data[i][j];
        while (--CubeCount)
        {
          AddCube(i, CubeCount, j);
        }
        AddCube(i, 0, j);
      }
    }
  }
}

void ChangeView()
{
begin:
  String ViewTypeTable[3] PROGMEM = {
    "Main-View",
    "Left-View",
    "Top-View"
  };
  String ViewType = ViewTypeTable[0];
  uint8_t WhichView = VIEW_MAIN;
  uint8_t pos_x = 0, pos_y = 0;
  uint8_t ViewFlg[3] = {0};
  //A键：长按向上，短按向下
  //B键：短按选择、清除，长按切换视图
  //C键：长按向左，短按向右
  //A、C同时按：完成输入
reDraw:
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("Input " + ViewType);

  for (int8_t i = 0; i < 5; ++i)
  {
    for (int8_t j = 0; j < 5; ++j)
    {
      M5.Lcd.fillRect(i * 30 + 2, j * 30 + 2 + 50, 26, 26, viewTable[WhichView][i][j] ? LIGHTGREY : DARKGREY);
    }
  }
  M5.Lcd.drawRect(pos_x * 30 - 1, pos_y * 30 - 1 + 50, 28, 28, TFT_WHITE);
  while (1)
  {
    M5.update();
    if (M5.BtnA.isPressed())
    {
      delay(40);
      if (M5.BtnC.isPressed())
      {
        while (!M5.BtnA.wasReleased()) M5.update();
        delay(100);
        M5.BtnC.wasReleased();
        break;
      }
    }
    if (M5.BtnA.pressedFor(LONGPRESS_TIME))
    {
      if (pos_x == 0)pos_x = 5;
      pos_x--;
      while (!M5.BtnA.wasReleased()) M5.update();
      delay(10);
      goto reDraw;
    }
    if (M5.BtnA.wasReleased())
    {
      if (pos_x == 4) {
        pos_x = 0;
        delay(10);
        goto reDraw;
      }
      pos_x++;
      delay(10);
      goto reDraw;
    }

    if (M5.BtnB.pressedFor(LONGPRESS_TIME))
    {
      WhichView ++;
      if (WhichView == 3)
      {
        WhichView = 0;
      }
      ViewType = ViewTypeTable[WhichView];
      while (!M5.BtnB.wasReleased()) M5.update();
      delay(10);
      goto reDraw;
    }
    if (M5.BtnB.wasReleased())
    {
      if (viewTable[WhichView][pos_x][pos_y] == 0)
      {
        viewTable[WhichView][pos_x][pos_y] = 1;
      }
      else
      {
        viewTable[WhichView][pos_x][pos_y] = 0;
      }

      delay(10);
      goto reDraw;
    }
    if (M5.BtnC.pressedFor(LONGPRESS_TIME))
    {
      if (pos_y == 0)pos_y = 5;
      pos_y--;
      while (!M5.BtnC.wasReleased()) M5.update();
      delay(10);
      goto reDraw;
    }
    if (M5.BtnC.wasReleased())
    {
      if (pos_y == 4) {
        pos_y = 0;
        delay(10);
        goto reDraw;
      }
      pos_y++;
      delay(10);
      goto reDraw;
    }
  }

  //检测已知视图
  for (WhichView = 0; WhichView < 3; ++WhichView)
  {
    for (uint8_t i = 0; i < 5; ++i)
    {
      for (uint8_t j = 0; j < 5; ++j)
      {
        if (viewTable[WhichView][i][j])
        {
          ViewFlg[WhichView] = 1;
        }
      }
    }
  }
  if (ViewFlg[0] == ViewFlg[1] == ViewFlg[2] == 1)
  {
    Calculate3V();
  }
  else if (ViewFlg[0] == ViewFlg[1] == 1 && ViewFlg[2] == 0)
  {
    CalculateML();
  }
  else if (ViewFlg[1] == ViewFlg[2] == 1 && ViewFlg[0] == 0)
  {
    CalculateLT();
  }
  else if (ViewFlg[0] == ViewFlg[2] == 1 && ViewFlg[1] == 0)
  {
    CalculateMT();
  }
  else
  {
    goto Error;
  }
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("Min:%u\nMax:%u\nPress BUTTON B to\n enter 3D mode\n", MinCube, MaxCube);
  delay(10);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.println("Main View Direction");
  M5.Lcd.setTextColor(TFT_BLUE);
  M5.Lcd.println("Left View Direction");
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.println("Top View Direction");
  while (digitalRead(BUTTON_B_PIN));
  delay(1000);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  Draw3DCube();
  return;
Error:
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.print("INPUT ERROR");
  delay(5000);
  M5.Lcd.setTextColor(TFT_WHITE);
  goto begin;
}
