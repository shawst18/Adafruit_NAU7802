/**************************************************************************/
/*!
  @file     Adafruit_NAU7802.cpp

  @mainpage Adafruit NAU7802 I2C 24-bit ADC driver

  @section intro Introduction

  This is a library for the Adafruit NAU7802 I2C ADC breakout board
  ----> http://www.adafruit.com/products/4538

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  @section author Author

  Limor Fied (Adafruit Industries)

  @section license License

  BSD (see license.txt)
*/
/**************************************************************************/
#include "Adafruit_NAU7802.h"

/**************************************************************************/
/*!
    @brief  Instantiates a new NAU7802 class
*/
/**************************************************************************/
Adafruit_NAU7802::Adafruit_NAU7802() {
}

/**************************************************************************/
/*!
    @brief  Sets up the I2C connection and tests that the sensor was found.
    @param theWire Pointer to an I2C device we'll use to communicate
    default is Wire
    @return true if sensor was found, otherwise false.
*/
/**************************************************************************/
bool Adafruit_NAU7802::begin(TwoWire *theWire) {
  i2c_dev = new Adafruit_I2CDevice(NAU7802_I2CADDR_DEFAULT, theWire);
  
  /* Try to instantiate the I2C device. */
  if (!i2c_dev->begin()) {
    return false;
  }
  
  /* Check for NAU7802 revision register (0x1F), low nibble should be 0xF. */
  Adafruit_I2CRegister rev_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_REVISION_ID);

  if ((rev_reg.read() & 0xF) != 0xF) {
    return false;
  }

  // define the main power control register
  _pu_ctrl_reg = new Adafruit_I2CRegister(i2c_dev, NAU7802_PU_CTRL);

  if (! reset()) return false;
  if (! setLDO(NAU7802_3V0)) return false;
  if (! setGain(NAU7802_GAIN_128)) return false;
  if (! setRate(NAU7802_RATE_10SPS)) return false;
  if (! enable(true)) return false;
  
  

  return true;
}

bool Adafruit_NAU7802::enable(bool flag) {
  Adafruit_I2CRegisterBits pu_analog =
      Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 2); // # bits, bit_shift
  Adafruit_I2CRegisterBits pu_digital =
      Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 1); // # bits, bit_shift
  Adafruit_I2CRegisterBits pu_ready =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 3); // # bits, bit_shift
  Adafruit_I2CRegisterBits pu_start =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 4); // # bits, bit_shift

  if (! flag) {
    // shut down;
    if (! pu_analog.write(0)) return false;
    if (! pu_digital.write(0)) return false;
    return true;
  }
  // turn on!
  if (! pu_digital.write(1)) return false;
  if (! pu_analog.write(1)) return false;
  // RDY: Analog part wakeup stable plus Data Ready after exiting power-down mode 600ms
  delay(600);
  if (! pu_start.write(1)) return false;
  return pu_ready.read(); 
}

bool Adafruit_NAU7802::available(void) {
  Adafruit_I2CRegisterBits conv_ready =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 5); // # bits, bit_shift
  return conv_ready.read();
}

int32_t Adafruit_NAU7802::read(void) {
  Adafruit_I2CRegister adc0 = Adafruit_I2CRegister(i2c_dev, NAU7802_ADCO_B2, 3, MSBFIRST);
  uint32_t val = adc0.read();
  // extend sign bit
  if (val & 0x80000) { val |= 0xFF000000; }

  return val;
}

bool Adafruit_NAU7802::reset(void) {
  Adafruit_I2CRegisterBits reg_reset =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 0); // # bits, bit_shift
  Adafruit_I2CRegisterBits pu_digital =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 1); // # bits, bit_shift
  Adafruit_I2CRegisterBits pu_ready =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 3); // # bits, bit_shift

  // Set the RR bit to 1 in R0x00, to guarantee a reset of all register values.
  if (!reg_reset.write(1)) return false;
  delay(10);
  // Set the RR bit to 0 and PUD bit 1, in R0x00, to enter normal operation
  if (!reg_reset.write(0)) return false;
  if (!pu_digital.write(1)) return false;
  // After about 200 microseconds, the PWRUP bit will be Logic=1 indicating the device is ready for the remaining programming setup.
  delay(1);
  return pu_ready.read(); 
}
  
bool Adafruit_NAU7802::setLDO(NAU7802_LDOVoltage voltage) {
  Adafruit_I2CRegisterBits reg_avdds =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 7); // # bits, bit_shift

  Adafruit_I2CRegister ctrl1_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL1);
  Adafruit_I2CRegisterBits vldo =
    Adafruit_I2CRegisterBits(&ctrl1_reg, 3, 3); // # bits, bit_shift

  if (voltage == NAU7802_EXTERNAL) {
    // special case!
    return reg_avdds.write(0);
  }
  
  // internal LDO
  if (! reg_avdds.write(1)) return false;
  return vldo.write(voltage);
}


NAU7802_LDOVoltage Adafruit_NAU7802::getLDO(void) {
  Adafruit_I2CRegisterBits reg_avdds =
    Adafruit_I2CRegisterBits(_pu_ctrl_reg, 1, 7); // # bits, bit_shift

  Adafruit_I2CRegister ctrl1_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL1);
  Adafruit_I2CRegisterBits vldo =
    Adafruit_I2CRegisterBits(&ctrl1_reg, 3, 3); // # bits, bit_shift

  if (! reg_avdds.read()) {
    return NAU7802_EXTERNAL;
  }
  // internal LDO
  return (NAU7802_LDOVoltage)vldo.read();
}

bool Adafruit_NAU7802::setGain(NAU7802_Gain gain) {
  Adafruit_I2CRegister ctrl1_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL1);
  Adafruit_I2CRegisterBits gain_select =
    Adafruit_I2CRegisterBits(&ctrl1_reg, 3, 0); // # bits, bit_shift

  return gain_select.write(gain);
}

NAU7802_Gain Adafruit_NAU7802::getGain(void) {
  Adafruit_I2CRegister ctrl1_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL1);
  Adafruit_I2CRegisterBits gain_select =
    Adafruit_I2CRegisterBits(&ctrl1_reg, 3, 0); // # bits, bit_shift

  return gain_select.read();
}


bool Adafruit_NAU7802::setRate(NAU7802_SampleRate rate) {
  Adafruit_I2CRegister ctrl2_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL2);
  Adafruit_I2CRegisterBits rate_select =
    Adafruit_I2CRegisterBits(&ctrl2_reg, 3, 4); // # bits, bit_shift

  return rate_select.write(rate);
}

NAU7802_SampleRate Adafruit_NAU7802::getRate(void) {
  Adafruit_I2CRegister ctrl2_reg = Adafruit_I2CRegister(i2c_dev, NAU7802_CTRL2);
  Adafruit_I2CRegisterBits rate_select =
    Adafruit_I2CRegisterBits(&ctrl2_reg, 3, 4); // # bits, bit_shift

  return (NAU7802_SampleRate)rate_select.read();
}
