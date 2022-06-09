#ifndef QUADKEYTILESYSTEM_H
#define QUADKEYTILESYSTEM_H

#include <math.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <iterator>
#include <array>

namespace QuadKeys
{
    class QuadKeyTSSimple
    {
        private:
        	double Clip(double n, double minValue, double maxValue);
            unsigned int MapSize(int levelOfDetail);
			int m_levelOfDetail;

        public:
        	QuadKeyTSSimple();
        	void setLevelOfDetail(int levelOfDetail = 16);
        	std::string LatLonToQuadKey(double latitude, double longitude);
    };
}

#endif // QUADKEYTILESYSTEM_H
