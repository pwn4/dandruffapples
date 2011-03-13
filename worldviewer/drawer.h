#ifndef DRAWER_H_
#define DRAWER_H_

#include <netinet/in.h>
#include <cairo.h>
#include <stdio.h>
#include "../common/globalconstants.h"
#include "../common/regionrender.pb.h"

struct TwoInt{
  int one, two;

  TwoInt(int first, int second) : one(first), two(second) {}
};

struct ColorObject{
  double r, g, b;

  ColorObject(double newr, double newg, double newb) : r(newr), g(newg), b(newb) {}
  ColorObject() : r(0), g(0), b(0){}
};

ColorObject colorFromTeam(int teamId);
void UnpackImage(cairo_t *cr, RegionRender* render, int robotSize, double robotAlpha);
unsigned int BytePack(int a, int b);
TwoInt ByteUnpack(unsigned int data);

#endif /* DRAWER_H_ */
