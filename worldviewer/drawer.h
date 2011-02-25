#ifndef DRAWER_H_
#define DRAWER_H_

#include <netinet/in.h>
#include <cairo.h>
#include <stdio.h>
#include "../common/imageconstants.h"
#include "../common/regionrender.pb.h"

struct TwoInt{
  int one, two;

  TwoInt(int first, int second) : one(first), two(second) {}
};

struct ColorObject{
  double r, g, b;

  ColorObject(double newr, double newg, double newb) : r(newr), g(newg), b(newb) {}
};

ColorObject colorFromTeam(int teamId);
cairo_surface_t * UnpackImage(RegionRender render);
unsigned int BytePack(int a, int b);
TwoInt ByteUnpack(unsigned int data);

#endif /* DRAWER_H_ */
