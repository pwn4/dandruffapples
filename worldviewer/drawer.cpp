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

		// set colors for the first few teams so that when there are only 2 teams we can tell the difference
		coloringMap[0] = ColorObject(0.094, 0.353, 0.420);
		coloringMap[1] = ColorObject(0.698, 0.051, 0.263);
		coloringMap[2] = ColorObject(0.898, 0.839, 0.157);
		coloringMap[3] = ColorObject(0.424, 0.698, 0.051);
		coloringMap[4] = ColorObject(0.039, 0.149, 0.176);

		colorMapInitialized = true;
	}

  //protection
  if(teamId < 0 || teamId >= 65535)
    return coloringMap[55555];

	return coloringMap[teamId];
}
void UnpackImage(cairo_t *cr, RegionRender* render, float drawFactor, double robotAlpha, WorldInfo *worldinfo,
		unsigned int regionId) {
	int curY = 0;
	ColorObject color;

  //we want those goddamn region boundaries, dammit!!
  cairo_rectangle (cr, 0, 0, IMAGEWIDTH* drawFactor, IMAGEHEIGHT* drawFactor);
  cairo_set_source_rgb(cr, .5, .5, .5);
  cairo_stroke (cr);

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
			
			//draw the home's score just below it. Yes, even if that shit cuts off. Egor, wanna make this better?
			cairo_text_extents_t te;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, 0.6);
			cairo_select_font_face (cr, "Arial", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size (cr, 30);

			for(int i = 0; i < render->score_size(); i++)
			{
			  if(render->score(i).team() == homeInfo->team())
			  {
			    std::ostringstream scoreString;
	        scoreString << render->score(i).score();
	        
          cairo_text_extents (cr, scoreString.str().c_str(), &te);
          cairo_move_to (cr, (homeInfo->home_x() - (te.width/2) - te.x_bearing) * drawFactor, (homeInfo->home_y() - (te.height/2) - te.y_bearing) * drawFactor);
	        
          cairo_show_text (cr, scoreString.str().c_str());
          break;
        }
      }
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
