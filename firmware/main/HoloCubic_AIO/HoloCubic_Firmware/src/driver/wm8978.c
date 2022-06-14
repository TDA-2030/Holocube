#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "wm8978.h"

static const char *TAG = "wm8978";

#define WM8978_ADDR   0X1A

// WM8978 register value buffer zone (total 58 registers 0 to 57), occupies 116 bytes of memory
// Because the IIC WM8978 operation does not support read operations, so save all the register values in the local
// Write WM8978 register, synchronized to the local register values, register read, register directly back locally stored value.
// Note: WM8978 register value is 9, so use uint16_t storage.

static uint16_t REGVAL_TBL[58] = {
    0X0000, 0X0000, 0X0000, 0X0000, 0X0050, 0X0000, 0X0140, 0X0000,
    0X0000, 0X0000, 0X0000, 0X00FF, 0X00FF, 0X0000, 0X0100, 0X00FF,
    0X00FF, 0X0000, 0X012C, 0X002C, 0X002C, 0X002C, 0X002C, 0X0000,
    0X0032, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
    0X0038, 0X000B, 0X0032, 0X0000, 0X0008, 0X000C, 0X0093, 0X00E9,
    0X0000, 0X0000, 0X0000, 0X0000, 0X0003, 0X0010, 0X0010, 0X0100,
    0X0100, 0X0002, 0X0001, 0X0001, 0X0039, 0X0039, 0X0039, 0X0039,
    0X0001, 0X0001
};

//WM8978写寄存器
//reg:寄存器地址
//val:要写入寄存器的值
//返回值:0,成功;
//其他,错误代码
static uint8_t WM8978_Write_Reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (reg << 1) | ((val >> 8) & 0X01);
    buf[1] = val & 0XFF;

    // Wire.beginTransmission(WM8978_ADDR); //发送数据到设备号为4的从机
    // Wire.write((const uint8_t *)buf, 2);
    // Wire.endTransmission();    // 停止发送
    i2c_cmd_handle_t cmd;
// retry:
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (WM8978_ADDR << 1) | 0, 1);
    i2c_master_write(cmd, buf, 2, 1);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "send reg(%d) err(%s)", reg, esp_err_to_name(ret));
        // vTaskDelay(pdMS_TO_TICKS(100));
        // goto retry;
    }

    REGVAL_TBL[reg] = val; //保存寄存器值到本地
    return 0;
}

//WM8978 init
//返回值:0,初始化正常
//    其他,错误代码
static uint8_t WM8978_Init(void)
{
    uint8_t res;
    res = WM8978_Write_Reg(0, 0);  //软复位WM8978
    if (res) {
        return 1;    //发送指令失败,WM8978异常
    }
    //以下为通用设置
    WM8978_Write_Reg(1, 0X1B); //R1,MICEN设置为1(MIC使能),BIASEN设置为1(模拟器工作),VMIDSEL[1:0]设置为:11(5K)
    WM8978_Write_Reg(2, 0X1B0);  //R2,ROUT1,LOUT1输出使能(耳机可以工作),BOOSTENR,BOOSTENL使能
    WM8978_Write_Reg(3, 0X6C); //R3,LOUT2,ROUT2输出使能(喇叭工作),RMIX,LMIX使能
    WM8978_Write_Reg(6, 0);    //R6,MCLK由外部提供
    WM8978_Write_Reg(43, 1 << 4);  //R43,INVROUT2反向,驱动喇叭
    WM8978_Write_Reg(47, 1 << 8);  //R47设置,PGABOOSTL,左通道MIC获得20倍增益
    WM8978_Write_Reg(48, 1 << 8);  //R48设置,PGABOOSTR,右通道MIC获得20倍增益
    WM8978_Write_Reg(49, 1 << 1);  //R49,TSDEN,开启过热保护
    WM8978_Write_Reg(10, 1 << 3);  //R10,SOFTMUTE关闭,128x采样,最佳SNR
    WM8978_Write_Reg(14, 1 << 3 | 1 << 8);  //R14,ADC 128x采样率 and enable high pass filter (3.7Hz cut-off)
    return 0;
}

// WM8978 read register
// Reads the value  of the local register buffer zone
// reg: Register Address
// Return Value: Register value
static uint16_t WM8978_Read_Reg(uint8_t reg)
{
    return REGVAL_TBL[reg];
}
//WM8978 DAC/ADC配置
//adcen:adc使能(1)/关闭(0)
//dacen:dac使能(1)/关闭(0)
void WM8978_cfgADDA(uint8_t dacen, uint8_t adcen)
{
    uint16_t regval;
    regval = WM8978_Read_Reg(3); //读取R3
    if (dacen) {
        regval |= 3 << 0;    //R3最低2个位设置为1,开启DACR&DACL
    } else {
        regval &= ~(3 << 0);    //R3最低2个位清零,关闭DACR&DACL.
    }
    WM8978_Write_Reg(3, regval); //设置R3
    regval = WM8978_Read_Reg(2); //读取R2
    if (adcen) {
        regval |= 3 << 0;    //R2最低2个位设置为1,开启ADCR&ADCL
    } else {
        regval &= ~(3 << 0);    //R2最低2个位清零,关闭ADCR&ADCL.
    }
    WM8978_Write_Reg(2, regval); //设置R2
}
//WM8978 输入通道配置
//micen:MIC开启(1)/关闭(0)
//lineinen:Line In开启(1)/关闭(0)
//auxen:aux开启(1)/关闭(0)
void WM8978_cfgInput(uint8_t micen, uint8_t lineinen, uint8_t auxen)
{
    uint16_t regval;
    regval = WM8978_Read_Reg(2); //读取R2
    if (micen) {
        regval |= 3 << 2;    //开启INPPGAENR,INPPGAENL(MIC的PGA放大)
    } else {
        regval &= ~(3 << 2);    //关闭INPPGAENR,INPPGAENL.
    }
    WM8978_Write_Reg(2, regval); //设置R2

    regval = WM8978_Read_Reg(44);  //读取R44
    if (micen) {
        regval |= 3 << 4 | 3 << 0;    //开启LIN2INPPGA,LIP2INPGA,RIN2INPPGA,RIP2INPGA.
    } else {
        regval &= ~(3 << 4 | 3 << 0);    //关闭LIN2INPPGA,LIP2INPGA,RIN2INPPGA,RIP2INPGA.
    }
    WM8978_Write_Reg(44, regval); //设置R44

    if (lineinen) {
        WM8978_setLINEINgain(5);    //LINE IN 0dB增益
    } else {
        WM8978_setLINEINgain(0);    //关闭LINE IN
    }
    if (auxen) {
        WM8978_setAUXgain(7);    //AUX 6dB增益
    } else {
        WM8978_setAUXgain(0);    //关闭AUX输入
    }
}
//WM8978 输出配置
//dacen:DAC输出(放音)开启(1)/关闭(0)
//bpsen:Bypass输出(录音,包括MIC,LINE IN,AUX等)开启(1)/关闭(0)
void WM8978_cfgOutput(uint8_t dacen, uint8_t bpsen)
{
    uint16_t regval = 0;
    if (dacen) {
        regval |= 1 << 0;    //DAC输出使能
    }
    if (bpsen) {
        regval |= 1 << 1;   //BYPASS使能
        regval |= 5 << 2;   //0dB增益
    }
    WM8978_Write_Reg(50, regval); //R50设置
    WM8978_Write_Reg(51, regval); //R51设置
}
//WM8978 MIC增益设置(不包括BOOST的20dB,MIC-->ADC输入部分的增益)
//gain:0~63,对应-12dB~35.25dB,0.75dB/Step
void WM8978_setMICgain(uint8_t gain)
{
    gain &= 0X3F;
    WM8978_Write_Reg(45, gain);    //R45,左通道PGA设置
    WM8978_Write_Reg(46, gain | 1 << 8); //R46,右通道PGA设置
}
//WM8978 L2/R2(也就是Line In)增益设置(L2/R2-->ADC输入部分的增益)
//gain:0~7,0表示通道禁止,1~7,对应-12dB~6dB,3dB/Step
void WM8978_setLINEINgain(uint8_t gain)
{
    uint16_t regval;
    gain &= 0X07;
    regval = WM8978_Read_Reg(47);  //读取R47
    regval &= ~(7 << 4);      //清除原来的设置
    WM8978_Write_Reg(47, regval | gain << 4); //设置R47
    regval = WM8978_Read_Reg(48);  //读取R48
    regval &= ~(7 << 4);      //清除原来的设置
    WM8978_Write_Reg(48, regval | gain << 4); //设置R48
}
//WM8978 AUXR,AUXL(PWM音频部分)增益设置(AUXR/L-->ADC输入部分的增益)
//gain:0~7,0表示通道禁止,1~7,对应-12dB~6dB,3dB/Step
void WM8978_setAUXgain(uint8_t gain)
{
    uint16_t regval;
    gain &= 0X07;
    regval = WM8978_Read_Reg(47);  //读取R47
    regval &= ~(7 << 0);      //清除原来的设置
    WM8978_Write_Reg(47, regval | gain << 0); //设置R47
    regval = WM8978_Read_Reg(48);  //读取R48
    regval &= ~(7 << 0);      //清除原来的设置
    WM8978_Write_Reg(48, regval | gain << 0); //设置R48
}
//设置I2S工作模式
//fmt:0,LSB(右对齐);1,MSB(左对齐);2,飞利浦标准I2S;3,PCM/DSP;
//len:0,16位;1,20位;2,24位;3,32位;
void WM8978_cfgI2S(uint8_t fmt, uint8_t len)
{
    fmt &= 0X03;
    len &= 0X03; //限定范围
    WM8978_Write_Reg(4, (fmt << 3) | (len << 5));  //R4,WM8978工作模式设置
}

//设置耳机左右声道音量
//voll:左声道音量(0~63)
//volr:右声道音量(0~63)
void WM8978_setHPvol(uint8_t voll, uint8_t volr)
{
    voll &= 0X3F;
    volr &= 0X3F; //限定范围
    if (voll == 0) {
        voll |= 1 << 6;    //音量为0时,直接mute
    }
    if (volr == 0) {
        volr |= 1 << 6;    //音量为0时,直接mute
    }
    WM8978_Write_Reg(52, voll);      //R52,耳机左声道音量设置
    WM8978_Write_Reg(53, volr | (1 << 8)); //R53,耳机右声道音量设置,同步更新(HPVU=1)
}
//设置喇叭音量
//voll:左声道音量(0~63)
void WM8978_setSPKvol(uint8_t volx)
{
    volx &= 0X3F; //限定范围
    if (volx == 0) {
        volx |= 1 << 6;    //音量为0时,直接mute
    }
    WM8978_Write_Reg(54, volx);      //R54,喇叭左声道音量设置
    WM8978_Write_Reg(55, volx | (1 << 8)); //R55,喇叭右声道音量设置,同步更新(SPKVU=1)
}

//设置3D环绕声
//depth:0~15(3D强度,0最弱,15最强)
void WM8978_set3D(uint8_t depth)
{
    depth &= 0XF; //限定范围
    WM8978_Write_Reg(41, depth); //R41,3D环绕设置
}

//设置EQ/3D作用方向
//dir:0,在ADC起作用
//    1,在DAC起作用(默认)
void WM8978_set3Ddir(uint8_t dir)
{
    uint16_t regval;
    regval = WM8978_Read_Reg(0X12);
    if (dir) {
        regval |= 1 << 8;
    } else {
        regval &= ~(1 << 8);
    }
    WM8978_Write_Reg(18, regval); //R18,EQ1的第9位控制EQ/3D方向
}

//设置EQ1
//cfreq:截止频率,0~3,分别对应:80/105/135/175Hz
//gain:增益,0~24,对应-12~+12dB
void WM8978_setEQ1(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval;
    cfreq &= 0X3; //限定范围
    if (gain > 24) {
        gain = 24;
    }
    gain = 24 - gain;
    regval = WM8978_Read_Reg(18);
    regval &= 0X100;
    regval |= cfreq << 5; //设置截止频率
    regval |= gain;   //设置增益
    WM8978_Write_Reg(18, regval); //R18,EQ1设置
}
//设置EQ2
//cfreq:中心频率,0~3,分别对应:230/300/385/500Hz
//gain:增益,0~24,对应-12~+12dB
void WM8978_setEQ2(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0X3; //限定范围
    if (gain > 24) {
        gain = 24;
    }
    gain = 24 - gain;
    regval |= cfreq << 5; //设置截止频率
    regval |= gain;   //设置增益
    WM8978_Write_Reg(19, regval); //R19,EQ2设置
}
//设置EQ3
//cfreq:中心频率,0~3,分别对应:650/850/1100/1400Hz
//gain:增益,0~24,对应-12~+12dB
void WM8978_setEQ3(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0X3; //限定范围
    if (gain > 24) {
        gain = 24;
    }
    gain = 24 - gain;
    regval |= cfreq << 5; //设置截止频率
    regval |= gain;   //设置增益
    WM8978_Write_Reg(20, regval); //R20,EQ3设置
}
//设置EQ4
//cfreq:中心频率,0~3,分别对应:1800/2400/3200/4100Hz
//gain:增益,0~24,对应-12~+12dB
void WM8978_setEQ4(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0X3; //限定范围
    if (gain > 24) {
        gain = 24;
    }
    gain = 24 - gain;
    regval |= cfreq << 5; //设置截止频率
    regval |= gain;   //设置增益
    WM8978_Write_Reg(21, regval); //R21,EQ4设置
}
//设置EQ5
//cfreq:中心频率,0~3,分别对应:5300/6900/9000/11700Hz
//gain:增益,0~24,对应-12~+12dB
void WM8978_setEQ5(uint8_t cfreq, uint8_t gain)
{
    uint16_t regval = 0;
    cfreq &= 0X3; //限定范围
    if (gain > 24) {
        gain = 24;
    }
    gain = 24 - gain;
    regval |= cfreq << 5; //设置截止频率
    regval |= gain;   //设置增益
    WM8978_Write_Reg(22, regval); //R22,EQ5设置
}

void WM8978_setALC(uint8_t enable, uint8_t maxgain, uint8_t mingain)
{
    uint16_t regval;

    if (maxgain > 7) {
        maxgain = 7;
    }
    if (mingain > 7) {
        mingain = 7;
    }

    regval = WM8978_Read_Reg(32);
    if (enable) {
        regval |= (3 << 7);
    }
    regval |= (maxgain << 3) | (mingain << 0);
    WM8978_Write_Reg(32, regval);
}

void WM8978_setNoise(uint8_t enable, uint8_t gain)
{
    uint16_t regval;

    if (gain > 7) {
        gain = 7;
    }

    regval = WM8978_Read_Reg(35);
    regval = (enable << 3);
    regval |= gain;   //设置增益
    WM8978_Write_Reg(35, regval); //R18,EQ1设置
}

void WM8978_setHPF(uint8_t enable)
{
    uint16_t regval;

    regval = WM8978_Read_Reg(14);
    regval &= ~(1 << 8);
    regval |= (enable << 8);
    WM8978_Write_Reg(14, regval); //R14,high pass filter
}


bool WM8978_begin()
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (WM8978_ADDR << 1) | 0, 1);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "No WM8978 dac @ i2c address: 0x%X", WM8978_ADDR);
        return false;
    }
    const int err = WM8978_Init();
    if (err) {
        ESP_LOGE(TAG, "WM8978 init err: 0x%X", err);
        return false;
    }
    WM8978_cfgI2S(2, 0); //Philips 16bit
    WM8978_cfgADDA(1, 1);   //Enable ADC DAC
    WM8978_cfgInput(1, 0, 0);  //mic, linein, aux
    WM8978_setMICgain(50);
    WM8978_setAUXgain(0);
    WM8978_setLINEINgain(0);
    WM8978_setSPKvol(50); //0-63
    WM8978_setHPvol(0, 0); //0-63
    WM8978_set3Ddir(0);
    WM8978_setEQ1(0, 24);
    WM8978_setEQ2(0, 24);
    WM8978_setEQ3(0, 24);
    WM8978_setEQ4(0, 24);
    WM8978_setEQ5(0, 24);
    WM8978_cfgOutput(1, 0); //Output enabled, bypass disabled
    return true;
}

// void WM8978_PlayMode(void)
// {
//     WM8978_ADDA_Cfg(1, 0);      //开启DAC
//     WM8978_Input_Cfg(0, 0, 0);  //关闭输入通道
//     WM8978_Output_Cfg(1, 0);    //开启DAC输出
//     WM8978_HPvol_Set(20, 20);
//     WM8978_SPKvol_Set(0x3F);
//     WM8978_MIC_Gain(0);
//     WM8978_I2S_Cfg(2, 0);       //设置I2S接口模式
// }

// void WM8978_RecoMode(void)
// {
//     WM8978_ADDA_Cfg(0, 1);      //开启ADC
//     WM8978_Input_Cfg(1, 0, 0);  //开启输入通道(MIC)
//     WM8978_Output_Cfg(0, 1);    //开启BYPASS输出
//     WM8978_HPvol_Set(0, 0);
//     WM8978_SPKvol_Set(0);
//     WM8978_MIC_Gain(50);        //MIC增益设置
//     WM8978_I2S_Cfg(2, 0);       //飞利浦标准,16位数据长度
// }