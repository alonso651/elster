
/*
 Arduino library to read data from an Elster A100C electricity meter.

 Copyright (C) 2012 Dave Berkeley projects@rotwang.co.uk

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 USA
*/

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include <pins_arduino.h>
#endif

#include <elster.h>

#define BIT_PERIOD 416 // us

typedef uint32_t stamp;

#define BITS(t) (((t) + (BIT_PERIOD/2)) / BIT_PERIOD)

unsigned long bcdtol(const unsigned char* data, int bytes)
{
    unsigned long result = 0;
    for (int i = 0;  i < bytes; i++)
    {
        const unsigned char digit = *data++;
        result *= 100;
        result += 10 * (digit >> 4);
        result += digit & 0xF;
    }
    return result;
}

  /*
   *  Called on every InfraRed pulse, ie. every '1' bit.
   */

static stamp last_us;

void ElsterA100C::on_change(void)
{
  //  push the number of bit periods since the last interrupt to a buffer.
  const stamp us = micros();
  const int diff = us - last_us;
  last_us = us;

  const int bit_periods = BITS(diff);
  buff_add(& bits, bit_periods);
}

static ElsterA100C* meters[2];

void on_change_0(void)
{
    if (meters[0]) meters[0]->on_change();
}

void on_change_1(void)
{
    if (meters[1]) meters[1]->on_change();
}

void ElsterA100C::init(int irq)
{
    buff_init(& bits);
    buff_init(& bytes);

    meters[irq] = this;

    switch (irq) {
        case 0 : attachInterrupt(0, on_change_0, RISING);   break;
        case 1 : attachInterrupt(1, on_change_1, RISING);   break;
    }
}

  /*
   *  Decode bit stream
   */

int ElsterA100C::add_bit(int state)
{
    bit_data >>= 1;

    if (state)
        bit_data += 0x200;

    if (bit_data)
        bit_index += 1;

    if (bit_index < 10)
        return 0;

    if (bit_data & 1) // start bit
    {
        if (!state) // stop bit
        {
            const int d = (bit_data >> 1) & 0xFF;
            // high represents '0', so invert the bits
            buff_add(& bytes, (d ^ 0xFF));
            bit_data = bit_index = 0;
            return 1;
        }
    }

    //  bad frame
    bit_data = bit_index = 0;
    return 0;
}

void ElsterA100C::on_timeout()
{
  // too long since last bits count
  
  if (bit_index)
    while (bit_data) // flush with trailing '0' bits
      add_bit(0);  
  bit_data = bit_index = 0;
}

void ElsterA100C::on_bits(int bits)
{
  // called on each bits count.
  // the bit periods represent a '1' followed by N '0's.

  if (bits < 1)
  {
    on_timeout();
    return;
  }

  // the elapsed bit periods since last interrupt
  // represents a '1' followed by 0 or more '0's.  
  add_bit(1);
  for (; bits > 1; bits--)
    add_bit(0);
}

  /*
   *
   */
   
int ElsterA100C::good_cs(unsigned char cs, unsigned char check)
{
  // note : should be 'cs == check', but I'm seeing
  // systematic 1-bit errors which I can't track down.      

  
  // Serial.print("bcc=");
  // Serial.print(check, HEX);
  // Serial.print(" cs=");
  // Serial.print(cs, HEX);
  // Serial.print(" xor=");
  // Serial.print(cs ^ check, HEX);
  // Serial.print("\r\n");
  

  int bits = 0;
  int delta = cs ^ check;
  for (; delta; delta >>= 1)
    if (delta & 0x01)
      bits += 1;

  return bits < 2;
}

byte bcdToDec(byte val){
  return((val/16*10) + (val%16));
}

void ElsterA100C::good_packet()
{
    struct info* info = (struct info*) data;        
    byte aux;      
    char binario [9];
    //
    //handler(bcdtol(info->rate_1_reserved, 5));
    // for (int i = 0; i < sizeof[data]; ++i)
    // {
    //   Serial.print(data[i]);
    //   Serial.print(" ");
    // }
    Serial.print("product: ");
    Serial.println(info->product);

    Serial.print("firmware: ");
    Serial.println(info->firmware);

    aux = bcdToDec(info->mfg_serial);
    itoa(aux,binario,2);
    char* string = binario;
    Serial.print("mfg_serial (b): ");
    Serial.println(string);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->config_serial);
    itoa(aux,binario,2);
    char* string1 = binario;
    Serial.print("config_serial (b): ");
    Serial.println(string1);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }


    Serial.print("utility_serial: ");
    Serial.println(info->utility_serial);

    aux = bcdToDec(info->meter_definition);
    itoa(aux,binario,2);
    char* string2 = binario;
    Serial.print("meter_definition (bit field): ");
    Serial.println(string2);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    Serial.print("rate_1_import_kWh: ");
    handler(bcdtol(info->rate_1_import_kWh, 5));

    Serial.print("rate_1_reserved: ");
    handler(bcdtol(info->rate_1_reserved, 5));

    Serial.print("rate_1_reverse_kWh: ");
    handler(bcdtol(info->rate_1_reverse_kWh, 5));

    Serial.print("rate_2_import_kWh: ");
    handler(bcdtol(info->rate_2_import_kWh, 5));

    Serial.print("rate_2_reserved_kWh: ");
    handler(bcdtol(info->rate_2_reserved_kWh, 5));
    
    Serial.print("rate_2_reverse_kWh: ");
    handler(bcdtol(info->rate_2_reverse_kWh, 5));
    
    aux = bcdToDec(info->reserved_01);
    itoa(aux,binario,2);
    char* string3 = binario;
    Serial.print("reserved_01 (b): ");
    Serial.println(string3);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->status);
    itoa(aux,binario,2);
    char* string4 = binario;
    Serial.print("status (bit field): ");
    Serial.println(string4);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->error);
    itoa(aux,binario,2);
    char* string5 = binario;
    Serial.print("error (bit field): ");
    Serial.println(string5);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    Serial.print("anti_creep: ");
    handler(bcdtol(info->anti_creep, 3));

    Serial.print("rate_1_time: ");
    handler(bcdtol(info->rate_1_time, 3));

    Serial.print("rate_2_time: ");
    handler(bcdtol(info->rate_2_time, 3));

    Serial.print("power_up: ");
    handler(bcdtol(info->power_up, 3));

    aux = bcdToDec(info->power_fail);
    itoa(aux,binario,2);
    char* string6 = binario;
    Serial.print("power_fail (b): ");
    Serial.println(string6);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->watchdog);
    itoa(aux,binario,2);
    char* string7 = binario;
    Serial.print("watchdog (b): ");
    Serial.println(string7);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->reverse_warning);
    itoa(aux,binario,2);
    char* string8 = binario;
    Serial.print("reverse_warning (b): ");
    Serial.println(string8);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }

    aux = bcdToDec(info->reserved_02);
    itoa(aux,binario,2);
    char* string9 = binario;
    Serial.print("reserved_02 (b): ");
    Serial.println(string9);
    aux = 0;
    // binario = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < sizeof(binario); ++i)
    {
    	binario[i] = 0;
    }
}

unsigned char ElsterA100C::bcc(unsigned char cs, const unsigned char* data, int count)
{
    for (int i = 0; i < count; ++i)
        cs += *data++;
    return cs;
}

void ElsterA100C::on_data(unsigned char c)
{
    // match the packet header
    const unsigned char match[] = { 0x01, 0x00, sizeof(info), 0x02 };

    if (!reading)
    {
        // keep a log of the last 4 chars
        last_4[0] = last_4[1];
        last_4[1] = last_4[2];
        last_4[2] = last_4[3];
        last_4[3] = c;

        if (memcmp(match, last_4, sizeof(match)) == 0)
        {
            idx = 0;
            reading = 1;
            return;
        }

        return;
    }

    if (idx < sizeof(info))
    {
        data[idx++] = c;
        return;
    }

    const unsigned char etx = 0x03;
    if (idx == sizeof(info))
    {
        if (c != etx)
        {
            printf("etx=%02X\n", c);
            reading = idx = 0;
            return;
        }
        idx++;
        return;
    }                    

    unsigned char cs = 0x00;
    cs = bcc(cs, match, sizeof(match));
    cs = bcc(cs, data, sizeof(data));
    cs = bcc(cs, & etx, 1);
        
    if (good_cs(cs, c))
      good_packet();

    reading = 0;
}

int ElsterA100C::decode_bit_stream(void)
{
  // look for a pause in the bit stream
  const stamp us = micros();
  const int diff = us - last_us;
  if (BITS(diff) > 20)
    on_timeout();

  // read the bit stream  
  int bit_count;
  if (buff_get(& bits, & bit_count) < 0)
    return -1;

  on_bits(bit_count);

  // read the byte stream decoded above
  int byte_data;
  if (buff_get(& bytes, & byte_data) < 0)
    return -1;

  return byte_data;
}

// FIN
