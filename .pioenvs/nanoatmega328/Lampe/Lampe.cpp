#include "Lampe.h"

void Lampe::setLight(int light, int color, int value) {
  if (color == 3) {
    Tlc.set((light * 9), value);
    Tlc.set((light * 9) + 1, value);
    Tlc.set((light * 9) + 2, value);
    Tlc.set((light * 9) + 3, value);
    Tlc.set((light * 9) + 4, value);
    Tlc.set((light * 9) + 5, value);
    Tlc.set((light * 9) + 6, value);
    Tlc.set((light * 9) + 7, value);
    Tlc.set((light * 9) + 8, value);
  } else {
    Tlc.set((light * 9) + (color * 3), value);
    Tlc.set((light * 9) + (color * 3) + 1, value);
    Tlc.set((light * 9) + (color * 3) + 2, value);
  }
  
}

