#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yay0.h"

static const char *dec_data = "MP7 is an absolute mess. The boards are gimmicky again, but this time there's more bad gimmicks than good. The boards are either effective but boring, like Grand Canal and Bowser's Enchanted Inferno, or held back by either a bad gimmick or Bowser Time. The bonus stars are randomly chosen from a pool of 6, which makes one of the most strategic elements of the series dependent on random chance. Rewards for duels are decided by roulette after the minigame is over, which can potentially give you nothing, and will never give you stars. Bowser Time is a random penalty that absolutely destroys the strategy of some boards, notably Windmillville and Neon Heights, severely shaking up what it means to have a lead and plan around the board gimmick. Mic spaces function as a free doubling of your coins when on, and as worthless spaces that only waste time playing the same animation when off. Mic games turned on don't tell you to press R to make buttons show up, and turned off the variety of minigames is reduced. Character-specific orbs create a tier list, and with only 2 characters per orb that means that a 4 player game will be imbalanced from the start no matter what. Good items like Flutter Orbs can be as cheap as 5 coins in a shop and aren't horribly rare. As is the case in the previous couple games, coins earned from battle minigames influence coin star total, but the random bonus stars offset this effect (the \"idiot savant\" school of balance).";

static const unsigned char enc_data[] =
{
  0x59, 0x61, 0x79, 0x30, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x1c,
  0x00, 0x00, 0x00, 0x24, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdd, 0xff,
  0xff, 0xb0, 0x00, 0x00, 0x20, 0x11, 0x10, 0x35, 0x20, 0x39, 0x20, 0x00,
  0x43, 0x4f, 0x4e, 0x47, 0x52, 0x41, 0x54, 0x55, 0x4c, 0x41, 0x54, 0x49,
  0x4f, 0x4e, 0x20, 0x21, 0x0d, 0x0a, 0x49, 0x46, 0x20, 0x59, 0x4f, 0x55,
  0x20, 0x41, 0x4e, 0x41, 0x4c, 0x59, 0x53, 0x45, 0x20, 0x20, 0x0d, 0x0a,
  0x44, 0x49, 0x46, 0x46, 0x49, 0x43, 0x55, 0x4c, 0x54, 0x20, 0x54, 0x48,
  0x49, 0x53, 0x50, 0x52, 0x4f, 0x4d, 0x2c, 0x57, 0x45, 0x20, 0x57, 0x4f,
  0x55, 0x4c, 0x44, 0x0d, 0x0a, 0x20, 0x54, 0x45, 0x41, 0x43, 0x48, 0x2e,
  0x2a
};

int main(int argc, char **argv)
{
  unsigned char *data;
  size_t size = strlen(dec_data);
  int result;

  data = malloc(size);

  /* Test compression */
  result = yay0_compress(dec_data, strlen(dec_data), &data, &size);
  if (result != YAY0_OK)
    printf("Compression failed with error code %d\n", result);
  else
    printf("Compression successful: compressed %u bytes to %zu\n",
      sizeof(dec_data), size);

  /* Test decompression */
  result = yay0_decompress(enc_data, sizeof(enc_data), &data, &size);
  if (result != YAY0_OK)
    printf("Decompression failed with error code %d\n", result);
  else
    printf("Decompression successful: decompressed %zu bytes to %s\n",
      size, data);

  free(data);

  return result;
}
