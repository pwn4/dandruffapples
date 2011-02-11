#ifndef _AREAENGINE_H_
#define _AREAENGINE_H_


class AreaEngine {
protected:
int topx, topy;   //displacement of the region. allows us to use absolute coords everywhere
int robotRatio, regionRatio;    //robotDiameter:puckDiameter, regionSideLength:puckDiameter
int** puckArray;
int** robotArray;
  
public:
  AreaEngine(int xdisp, int ydisp, int robotSize, int regionSize, int maxMemory);
  ~AreaEngine();

};

#endif
