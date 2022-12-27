/* Copyright (c) 2011, 2018 Dirk-Willem van Gulik, All Rights Reserved.
                      dirkx(at)webweaving(dot)org

   This file is licensed to you under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with
   the License.  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

   See the License for the specific language governing permissions and
   limitations under the License.

*/

// hardcoded to 30fps / EBU mode with no time offset or date -- as the
// latter seemed to confuse some clocks.
//
unsigned char   user[8] = {
  0, //user - bits field 1(2)
  0, // colour(2), undef
  0, //user bits 3(6)
  0, //user bits 4(8)
  0,
  0,
  0,
  0,
};


void incsmpte(int fps);



#define BCD(x) (((int)(x/10)<<4) | (x % 10))
void fillNextBlock(unsigned char block[10], int fps)
{
  incsmpte(fps);
/*
  Serial.print(hourCount,HEX);
  Serial.print(":");
  Serial.print(minsCount,HEX);
  Serial.print(":");
  Serial.print(secsCount,HEX);
  Serial.print(":");
  Serial.print(frameCount,HEX);
  Serial.print(":");
  Serial.println(ms());
  */
  block[0] = (user[0] << 4) | (frameCount & 0xf);
  block[1] = (user[1] << 4) | (frameCount >> 4) | 0 /* drop frame */ | 0 /* color */;
  block[2] = (user[2] << 4) | (secsCount & 0xf);
  block[3] = (user[3] << 4) | (secsCount >> 4); /* parity bit set at the very end. */
  block[4] = (user[4] << 4) | (minsCount & 0xf);
  block[5] = (user[5] << 4) | (minsCount >> 4);
  block[6] = (user[6] << 4) | (hourCount & 0xf);
  block[7] = (user[7] << 4) | (hourCount >> 4);
  block[8] = 0xfc; // sync/detect/direction bytes
  block[9] = 0xbf; // sync/detect/direction bytes

  unsigned char   par, i;
  par = 1; //last two constants
  for (i = 0; i < 8; i++)
    par ^= block[i];
  par ^= par >> 4;
  par ^= par >> 2;
  par ^= par >> 1;

  if (par & 1)
    block[ (fps == 30) ? 3 : 7 ] |= 8;
}

void incsmpte(int fps)
{
  int hexfps = ((fps / 10) << 4) + (fps % 10); // 23 -> 0x23
  frameCount++;
  if ((frameCount & 0x0f) > 9)
    frameCount += 6;
  if (frameCount < hexfps)
    return;
  frameCount = 0;

  secsCount++;
  if ((secsCount & 0x0f) > 9)
    secsCount += 6;
  if (secsCount < 0x60)
    return;
  secsCount = 0;

  minsCount++;
  if ((minsCount & 0x0f) > 9)
    minsCount += 6;
  if (minsCount < 0x60)
    return;
  minsCount = 0;

  hourCount++;
  if ((hourCount & 0x0f) > 9)
    hourCount += 6;
  if (hourCount < 0x24)
    return;
  hourCount = 0;
}



void setTS(unsigned char _hour, unsigned char _min, unsigned char _sec) {
  hourCount = BCD(_hour);
  secsCount = BCD(_sec);
  minsCount = BCD(_min);
  Serial.printf("BCD Time %02x:%02x:%02x.%02x\n", hourCount, minsCount, secsCount, frameCount);
};

void _setTS(unsigned char _hour, unsigned char _min, unsigned char _sec, unsigned char _frame) {
  hourCount = BCD(_hour);
  secsCount = BCD(_sec);
  minsCount = BCD(_min);
  frameCount = BCD(_frame);
  Serial.printf("BCD Time %02x:%02x:%02x.%02x\n", hourCount, minsCount, secsCount, frameCount);
};
