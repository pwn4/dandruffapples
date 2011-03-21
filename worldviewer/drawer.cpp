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

void UnpackImage(cairo_t *cr, RegionRender* render, int robotSize, double robotAlpha, WorldInfo *worldinfo,
		unsigned int regionId) {
	int curY = 0;
	ColorObject color;

	//draw the homes separately
	cairo_set_line_width(cr, 3);

	HomeInfo *homeInfo;
	for (int i = 0; i < worldinfo->home_size(); i++) {

		homeInfo = worldinfo->mutable_home(i);

		if (homeInfo->region_id() == regionId) {
			color = colorFromTeam(homeInfo->team());
			cairo_set_source_rgb(cr, color.r, color.g, color.b);
			cairo_arc(cr, IMAGEWIDTH * ((double) homeInfo->home_x() / (double) REGIONSIDELEN),
					IMAGEHEIGHT * ((double) homeInfo->home_y() / (double) REGIONSIDELEN), HOMEDIAMETER / 8, 0, 2 * M_PI);
			cairo_stroke(cr);
		}
	}
	TwoInt curRobot;


	if (robotSize == 1) { //pixel precision robot drawing
		for (int i = 0; i < render->image_size(); i++) {
			curRobot = ByteUnpack(render->image(i));

			while (curRobot.one == 65535 && curRobot.two == 65535 && i < render->image_size()) {
				i++;
				if (i >= render->image_size())
					return;
				curRobot = ByteUnpack(render->image(i));
				curY++;
			}

			//set the color
			color = colorFromTeam(curRobot.two);

			cairo_set_source_rgba(cr, color.r, color.g, color.b, robotAlpha);
			cairo_rectangle(cr, curRobot.one, curY, robotSize, robotSize);
			cairo_fill(cr);
		}
	} else {
		cairo_pattern_t *radpat; //for drawing gradients with large robots
		for (int i = 0; i < render->image_size(); i++) {
			curRobot = ByteUnpack(render->image(i));

			while (curRobot.one == 65535 && curRobot.two == 65535 && i < render->image_size()) {
				i++;
				if (i >= render->image_size())
					return;
				curRobot = ByteUnpack(render->image(i));
				curY++;
			}

			//set the color
			color = colorFromTeam(curRobot.two);

			//aliased precision
			//init the gradient
			radpat = cairo_pattern_create_radial(curRobot.one, curY, 0.0, curRobot.one, curY, (double) robotSize / 2.0);
			cairo_pattern_add_color_stop_rgba(radpat, 0, color.r, color.g, color.b, robotAlpha);
			cairo_pattern_add_color_stop_rgba(radpat, (double) robotSize / 2.0, color.r, color.g, color.b, 0.0);

			cairo_rectangle(cr, curRobot.one - (robotSize / 2.0), curY - (robotSize / 2.0), robotSize, robotSize);
			cairo_set_source(cr, radpat);
			cairo_fill(cr);
			cairo_pattern_destroy(radpat);
		}
	}
}
