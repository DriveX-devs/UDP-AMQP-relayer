#include <string>
#include <iostream>
#include <sstream>
#include <cmath>
#include <fstream>
#include <numeric>
#include "quadkey_ts_simple.h"

namespace QuadKeys
{
	QuadKeyTSSimple::QuadKeyTSSimple() {
		// Set the default level of detail and corresponding needed lat-lon variation
		m_levelOfDetail = 16;
	}

	double
	QuadKeyTSSimple::Clip(double n, double minValue, double maxValue) {
		return std::min(std::max(n, minValue), maxValue);
	}

	unsigned int
	QuadKeyTSSimple::MapSize(int levelOfDetail) {
		return (unsigned int) 256 << levelOfDetail;
	}

	void
	QuadKeyTSSimple::setLevelOfDetail(int levelOfDetail) {
		m_levelOfDetail = levelOfDetail;

		if(levelOfDetail < 14){
			m_levelOfDetail = 14;
		}
		if(levelOfDetail > 18){
			m_levelOfDetail = 18;
		}
	}

	std::string
	QuadKeyTSSimple::LatLonToQuadKey(double latitude, double longitude) {
		std::stringstream quadKey;
		// int levelOfDetail = ...; // The value of the desired zoom is obtained directly from the private attribute

		double x = (longitude + 180) / 360;
		double sinLatitude = sin(latitude * M_PI / 180);
		double y = 0.5 - log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * M_PI);

		uint mapSize = MapSize(m_levelOfDetail);
		int pixelX = (int) Clip(x * mapSize + 0.5, 0, mapSize - 1);
		int pixelY = (int) Clip(y * mapSize + 0.5, 0, mapSize - 1);
		int tileX =  pixelX / 256;
		int tileY =  pixelY / 256;

		for (int i = m_levelOfDetail; i > 0; i--) {
			char digit = '0';
			int mask = 1 << (i - 1);
			if ((tileX & mask) != 0)
			{
				digit++;
			}
			if ((tileY & mask) != 0)
			{
				digit++;
				digit++;
			}
			quadKey << digit;
		}

		return quadKey.str();
	}

}