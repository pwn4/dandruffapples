#include "drawer.h"

//globals
ColorObject coloringMap[65535];
bool colorMapInitialized = false;

//this is the color mapping method: teams->Color components
ColorObject colorFromTeam(int teamId) {
	//initialize the color map for the first time
	if (colorMapInitialized == false) {
		//use the same seed always so that our colors are the same everywhere
		srand(1);

		for (int i = 0; i < 65534; i++)
			coloringMap[i] = ColorObject(0.01 * (rand() % 60) + 0.2, 0.01 * (rand() % 60) + 0.2,
					0.01 * (rand() % 60) + 0.2);

		//set puck color
		coloringMap[65534] = ColorObject(0, 0, 0);

		colorMapInitialized = true;
	}

	return coloringMap[teamId];
}
void UnpackImage(cairo_t *cr, RegionRender* render, float drawFactor, double robotAlpha, WorldInfo *worldinfo,
		unsigned int regionId) {
	int curY = 0;
	ColorObject color;

	//draw the homes separately
	cairo_set_line_width(cr, 2);

	HomeInfo *homeInfo;
	for (int i = 0; i < worldinfo->home_size(); i++) {

		homeInfo = worldinfo->mutable_home(i);

		if (homeInfo->region_id() == regionId) {
			color = colorFromTeam(homeInfo->team());
			cairo_set_source_rgb(cr, color.r, color.g, color.b);
			cairo_arc(cr, homeInfo->home_x() * drawFactor, homeInfo->home_y() * drawFactor, HOMEDIAMETER/2 * drawFactor, 0, 2 * M_PI);
			cairo_stroke(cr);
		}
	}

		for (int i = 0; i < render->image_size(); i++) {
			TwoInt curRobot = ByteUnpack(render->image(i));

			while (curRobot.one == 65535 && curRobot.two == 65535 && i < render->image_size()) {
				i++;
				if (i >= render->image_size())
					return;
				curRobot = ByteUnpack(render->image(i));
				curY++;
			}


			//cairo_rectangle(cr, curRobot.one, curY, 3, 3);
			if(curRobot.two == 65534 ){	//draw puck
				cairo_set_source_rgba(cr, 0, 0, 0, robotAlpha);
				//I wish I could PUCKDIAMETER/2
				cairo_arc(cr, curRobot.one*drawFactor, curY*drawFactor, std::max((float)MINDIAMETER, PUCKDIAMETER*drawFactor), 0, 2 * M_PI);
			}
			else{ //draw robot
				//set the color
				color = colorFromTeam(curRobot.two);
				cairo_set_source_rgba(cr, color.r, color.g, color.b, robotAlpha);
				cairo_arc(cr, curRobot.one*drawFactor, curY*drawFactor, std::max((float)MINDIAMETER, ROBOTDIAMETER/2*drawFactor), 0, 2 * M_PI);
			}

			cairo_fill(cr);
		}
}
