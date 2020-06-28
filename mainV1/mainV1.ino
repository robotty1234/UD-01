#include <Servo.h>
#include <SD.h>

#define chipSelect 53
#define CHARSIZE 64//serial.available()が64byteまで保持できないから
#define FILEAll 50
#define SVPORT 5
#define SVPIN 6
#define MAXstep 20
#define MODE 24
#define MODELED 22
#define WAIT 5
#define BPS 115200
#define timeSet 5

const uint8_t keys = 0x01;
const uint8_t moves = 0x01;
const uint8_t action = 0x02;
const uint8_t gos = 0x01;
const uint8_t back = 0x02;
const uint8_t left = 0x03;
const uint8_t right = 0x04;
const uint8_t stops = (uint8_t)0x00;
const uint8_t triger = 0x01;
const uint8_t sw0 = 0x02;
const byte lf = 0x0a;

struct information{
  uint8_t keyNum;
  uint8_t type;
  uint8_t commond;
  uint8_t sum;
};
struct information info;
struct information trans;

//メインボードのサーボピン一覧
const int pin[SVPORT][SVPIN] = {//{26,28,30,31,29,27},
                       {26,28,30,29,31,27},
                       {32,33,34,35,36,37},
                       {A12,A14,44,46,48,47},
                       {37,39,41,43,45,49},
                       {A9,A11,A13,A15,33,35}
                       };
//const float timeSet = 20.0;
char sent[CHARSIZE];
int chars=0;
struct svCod
{
  int timing;
  String sum;
  char com[1];
  String svString;
  String svPort;
  String svPin;
  String svDeg;
  String svStep;
  String motionNum;
};

int homePos[SVPORT][SVPIN];//ホームポジションを記録しておく配列

int ma[20][5][6];

File homeFile;

svCod svc = {0,{""},{""},{""},{""},{""}};

int motionArray[MAXstep][SVPORT][SVPIN];//ステップごとに各関節の角度を記録しておく配列
int backApp[SVPORT][SVPIN];//GUIから送られてきた関節の角度を記録しておく配列
int delayArray[MAXstep];
boolean programMode;
int portFile = 0;

Servo servo[SVPORT][SVPIN];

char syokiChar(char be[CHARSIZE])//char型をリセット
{
  char re[CHARSIZE] = {""};
  for(int j=0;j<CHARSIZE;j++){
    be[j] = re[j];
  }
}

void taskGUI(char serial[])//パソコンから指示を受けて仕事する関数
{
  Serial.println("startTask");
  switch(svc.timing)
  {
    case 0:
    svc.com[0] = serial[0];
    break;
    case 1:
    if(svc.com[0] == 'w' || svc.com[0] == 'a')
    {
      svc.svPort = serial;
    }
    else
    {
      svc.svString = serial;
    }
    break;
    case 2:
    if(svc.com[0] == 'w')
    {
      svc.svPin = serial;
    }
    else
    {
      svc.svStep = serial;
    }
    break;
    case 3:
    if(svc.com[0] == 'w')
    {
      svc.svDeg = serial;
    }
    else
    {
      svc.motionNum = serial;
    }
    break;
    case 4:
    svc.sum = serial;
    break;
  }
  svc.timing++;
  if(serial[0] == 'f')
  {
    svc.timing=0;
    if(svc.com[0] == 'w')//指定されたサーボを動かす
    {
      int svport = svc.svPort.toInt();
      int svpin = svc.svPin.toInt();
      int svdeg = svc.svDeg.toInt();
      int sumAll = svc.sum.toInt();
      if(sumAll = svport+svpin+svdeg)
      {
        backApp[svport][svpin] = svdeg;
      }
    }
    else if(svc.com[0] == 'h')//指定されたサーボのホームポジションを書き換える
    {
      remakeHome();
    }
    else if(svc.com[0] == 'm')//指定されたモーションファイルを書き換える
    {
      //Serial.println("モーション書き込み");
      remakeMotion();
    }
    else if(svc.com[0] == 't')//指定されたモーションファイルを再生
    {
      int svport = svc.svPort.toInt();
      moveServo(svport);
    }
  }
}

void remakeHome()//ホームポジションを書き換える関数
{
  char val[1000];
  if((svc.svStep.toInt())==0)
  {
    SD.remove("home.udh");
  }
  File homefile = SD.open("home.udh",FILE_WRITE);
  homefile.seek(homefile.size());
  homefile.println(svc.svString);
  delay(WAIT);
  homefile.close();
  Serial.println("Finish write to sdcard on homedata");
}
//モーションファイルの書き込み
void remakeMotion(){
  if((svc.svStep.toInt())== 0)
  {
    SD.remove("mv" + String(svc.motionNum.toInt()) + ".udm");
  }
  String motion = {"mv" + String(svc.motionNum.toInt()) + ".udm"};
  File motionfile = SD.open(motion,FILE_WRITE);
  motionfile.seek(motionfile.size());
  if(portFile >= SVPORT){
    motionfile.println(svc.svString);
    portFile = 0;
  }
  else{
    motionfile.print(svc.svString);
    portFile++;
  }
  delay(WAIT);
  motionfile.close();
  Serial.println("Finish write to sdcard on motiondata");
}

//ホームポジションの角度を読んで動かす関数
void homeRePosition()
{
  homeFile = SD.open("home.udh",FILE_READ);
  if(homeFile == true)//ファイルがあったら
  {
    Serial.println("Moving to homepotsion");
    int maxSIZE = homeFile.size();
    String provisional;
    int mvFin=0,times=0,port=0,pin=0,how=0;
    char chr[CHARSIZE];
    char reads[100];

    while(times<maxSIZE){
      /*Serial.print("mvFin");
      Serial.println(mvFin);
      Serial.flush();
      Serial.print("maxSIZE");
      Serial.println(maxSIZE);
      Serial.flush();*/
      reads[times] = homeFile.read();
      if(reads[times] == ',')//区切り
      {
        provisional = chr;
        syokiChar(chr);
        homePos[port][pin] = provisional.toInt();
        /*Serial.print("pin=");
        Serial.println(pin);
        Serial.flush();*/
        how = 0;
        pin++;
      }
      else if(reads[times] == '\n')//改行
      {
        //provisional = chr;
        syokiChar(chr);
        //homePos[port][pin] = provisional.toInt();
        /*Serial.print("port=");
        Serial.println(port);
        Serial.flush();*/
        pin = 0;
        port++;
        how = 0;
      }
      else//読み途中ならば
      {
        chr[how] = reads[times];
        how++;
      }
      times++;
    }
    for(int i=0;i<SVPORT;i++)
    {
      for(int j=0;j<SVPIN;j++)
      {
        servo[i][j].write(homePos[i][j]);
        Serial.print(homePos[i][j]);
        //Serial.println(homePos[i][j]);
      }
    }
    delay(10);
    homeFile.close();
    Serial.println("close");
    Serial.flush();
  }
  else//ホームポジションのファイルがなかったら
  {
    Serial.println("###error to read homepotsion file###");
    for(int i=0;i<SVPORT;i++){
      for(int j=0;j<SVPIN;j++){
        homePos[i][j] = 90;
      }
    }
  }
}

//ホームポジションの角度を動かす関数
void homePosition(int port,int pin,int deg,boolean juge)
{
  if(juge == true)//変数の変更があれば
  {
    homePos[port][pin] = deg;
  }
  Serial.println("###Moving to homepotsion###");
  for(int i=0;i<5;i++)
  {
    for(int j=0;j<6;j++)
    {
      servo[i][j].write(homePos[i][j]);
      backApp[i][j] = homePos[i][j];
    }
  }
  delay(10);
}

//サーボモータを動かす関数
void moveServo(int motNum)
{
  String provisional;
  int mvFin=0,times=0,port=0,pin=0,how=0,filePos=0,svTimes=0,delayValue;
  char chr[CHARSIZE];
  char reads[100];
  int svPosArr[SVPORT][SVPIN];
  int resvPosArr[SVPORT][SVPIN];
  String motName = {"mv" + String(motNum) + ".udm"};
  File motion = SD.open(motName,FILE_READ);
  if(motion == true)//ファイルがあったら
  {
    Serial.print(motNum);
    Serial.println("MoveStart!");
    int maxSIZE = motion.size();

    while(times<maxSIZE){
      reads[times] = motion.read();
      delay(WAIT);
      if(reads[times] == ',')//区切り
      {
        provisional = chr;
        syokiChar(chr);
        svPosArr[port][pin] = provisional.toInt();
        pin++;
        how=0;
        //Serial.print("svPosArr[0][0]フェーズ4:");
        //Serial.println(svPosArr[0][0]);
      }
      else if(reads[times] == '|')//ポートチェンジ
      {
        provisional = chr;
        syokiChar(chr);
        svPosArr[port][pin] = provisional.toInt();
        pin=0;
        port++;
        how=0;
        //Serial.print("svPosArr[0][0]フェーズ5:");
        //Serial.println(svPosArr[0][0]);
      }
      else if(reads[times] == '\n')//改行
      {
        //時間読み込み
        provisional = chr;
        syokiChar(chr);
        delayValue = provisional.toInt();
        pin = 0;
        port= 0;
        how = 0;
        //サーボを実際動かす
        Serial.print("時間:");
        Serial.print(delayValue);
        Serial.print(" ");
        for(int i=0;i<SVPORT;i++)
        {
          for(int j=0;j<SVPIN;j++)
          {
            Serial.print(homePos[i][j] + svPosArr[i][j]);
            Serial.print(",");
            Serial.flush();
          }
          Serial.print("|"); 
        }
        Serial.println("");
        //Serial.print("svPosArr[0][0]フェーズ6:");
        //Serial.println(svPosArr[0][0]);
        if(svTimes == 0)//はじめだけ
        {
          for(int t=0;t<=delayValue;t=t+timeSet)
          {
            for(int i=0;i<SVPORT;i++)
            {
              for(int j=0;j<SVPIN;j++)
              {
                servo[i][j].write(homePos[i][j]+(svPosArr[i][j]*t/delayValue));
              }
            }
            //Serial.print("svPosArr[0][0]フェーズ7:");
            //Serial.println(svPosArr[0][0]);
            delay(timeSet);
          }
        }
        else//初回起動以外
        {
          for(int t=0;t<=delayValue;t=t+timeSet)
          {
            for(int i=0;i<SVPORT;i++)
            {
              for(int j=0;j<SVPIN;j++)
              {
                servo[i][j].write((homePos[i][j]+resvPosArr[i][j])+((svPosArr[i][j]-resvPosArr[i][j])*t/delayValue));
              }
            }
            //Serial.print("svPosArr[0][0]フェーズ8:");
            //Serial.println(svPosArr[0][0]);
            delay(timeSet);
          }
        }
        //Serial.print("svPosArr[0][0]フェーズ1:");
        //Serial.println(svPosArr[0][0]);
        for(int i=0;i<SVPORT;i++)
        {
          for(int j=0;j<SVPIN;j++)
          {
            resvPosArr[i][j]=svPosArr[i][j];
          }
        }
        //Serial.print("svPosArr[0][0]フェーズ2:");
        //Serial.println(svPosArr[0][0]);
        svTimes++;
      }
      else//読み途中ならば
      {
        chr[how] = reads[times];
        how++;
      }
      //Serial.print("svPosArr[0][0]フェーズ3:");
      //Serial.println(svPosArr[0][0]);
      times++;
    }
    motion.close();
    Serial.println("close");
    Serial.flush();
  }
  else//ホームポジションのファイルがなかったら
  {
    Serial.println("###failed read homepositon file###");
  }
}

void motionPlay(uint8_t ty, uint8_t com) {
  /*Serial3.write(keys);
  Serial3.flush();
  Serial3.write((uint8_t)0x00);
  Serial3.flush();*/
  Serial3.end();
  switch(ty){
    case (uint8_t)0x00:
    switch(com){
      case (uint8_t)0x00:
        Serial.println("re conection");
        Serial3.begin(BPS);
        writeSerialUD(keys,(uint8_t)0x00,(uint8_t)0x00,lf);
        Serial3.end();
        break;

      default:
        break;
    }
    Serial.println("finish");
    break;
    
    case moves:
    switch (com) {
      case gos:
        Serial.println("go");
        moveServo(0);
        break;
      case back:
        Serial.println("back");
        moveServo(1);
        break;
      case left:
        Serial.println("turn left");
        moveServo(2);
        break;
      case right:
        Serial.println("turn right");
        moveServo(3);
        break;
      case stops:
        Serial.println("homeposition");
        homePosition(0,0,0,false);
        delay(100);
        break;
      default:
        break;
    }
    break; 

    case action:
    switch (com) {
      case triger:
        Serial.println("triger action");
        moveServo(4);
        break;
      case sw0:
        Serial.println("sw0 action");
        moveServo(5);
        break;
    }
    break; 

    default:
      break;
  }
  Serial3.begin(BPS);
}

void writeSerialUD(uint8_t k,uint8_t t,uint8_t c,uint8_t l){
  Serial3.write(k);
  Serial3.flush();
  Serial3.write(t);
  Serial3.flush();
  Serial3.write(c);
  Serial3.flush();
  Serial3.write(k+t+c);
  Serial3.flush();
  Serial3.write(l);
  Serial3.flush();
}

void readSerialUD(struct information *data){
  data->keyNum = Serial3.read();
  delay(WAIT);
  data->type = Serial3.read();
  delay(WAIT);
  data->commond = Serial3.read();
  delay(WAIT);
  data->sum = Serial3.read();
  delay(WAIT);
  Serial3.end();
  delay(WAIT);
  Serial3.begin(BPS);
}

void setup()
{
  pinMode(13,OUTPUT);
  digitalWrite(13,HIGH);
  delay(100);
  Serial.begin(9600);
  Serial3.begin(BPS);
  pinMode(SS,OUTPUT);
  pinMode(MODE,INPUT_PULLUP);
  pinMode(MODELED,OUTPUT);
  SD.begin(chipSelect);
  for(int i=0;i<SVPORT;i++)
  {
    for(int j=0;j<SVPIN;j++)
    {
      servo[i][j].attach(pin[i][j]);
    }
  }
   homeRePosition();
   for(int i=0;i<SVPORT;i++)
  {
    for(int j=0;j<SVPIN;j++)
    {
      Serial.print(homePos[i][j]);
      Serial.print(",");
    }
    Serial.println();
  }
  Serial.println("setupFinish");
  homePosition(0,0,0,false);
  if(digitalRead(MODE)==LOW){
    programMode = true;
  }else{
    programMode = false;
  }
  
  if(programMode == false){
    digitalWrite(MODELED,LOW);
    boolean LEDSignal;
    while(info.keyNum != keys){
      while(Serial3.available() <= 4){
        digitalWrite(MODELED,LEDSignal);
        LEDSignal = !LEDSignal;
        Serial.println("wait authentication");
        delay(300);
      }
      readSerialUD(&info);
      int kn = info.keyNum;
      Serial.print("ID:");
      Serial.println(kn);
    }
    writeSerialUD(keys,(uint8_t)0x00,(uint8_t)0x00,lf);
  }
  digitalWrite(13,LOW);
  delay(10);
  /*moveServo(0);
  homeRePosition();
  delay(1000);*/
}

void loop() 
{
  //int mode  = digitalRead(MODE);
  if(programMode == true)
  {
    digitalWrite(MODELED,HIGH);
    if(Serial.available())
    {
      char stay = Serial.read();
      delay(WAIT);
      if(stay == '\n' || chars > 1000)
      {
        taskGUI(sent);
        chars=0;
        syokiChar(sent);
        Serial.println("@next");//この文は必須
      }else
      {
        sent[chars] = stay;
        chars++;
      }
    }
    else{
      for(int po=0;po<SVPORT;po++){
        for(int pi=0;pi<SVPIN;pi++){
          servo[po][pi].write(backApp[po][pi]);
        }
      }
      delay(5);
    }
  }
  else
  {
    digitalWrite(MODELED,LOW);
    if(Serial3.available() > 4){
      readSerialUD(&info);
      if(info.keyNum == keys && info.sum == info.keyNum + info.type + info.commond){
        //Serial.println("通信中");
        motionPlay(info.type,info.commond);
      }
    }
  }
}
