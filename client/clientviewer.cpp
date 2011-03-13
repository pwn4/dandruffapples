#include "client.h"

int infoTimeCache, infoMessages, infoLastSecond;

//"About" toolbar button handler
void onAboutClicked(GtkWidget *widget, gpointer window) {
	GtkWidget *dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "Client Viewer");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.1");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "(c) Team 2");
	gtk_about_dialog_set_comments(
			GTK_ABOUT_DIALOG(dialog),
			"Client Viewer is a program to create a real-time visual representation of the a single robot as generated by the client.");
	const gchar
			*authors[2] = {
					"Peter Neufeld, Frank Lau, Egor Philippov,\nYouyou Yang, Jianfeng Hu, Roy Chiang,\nWilson Huynh, Gordon Leung, Kevin Fahy,\nBenjamin Saunders",
					NULL };

	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

//window destruction methods for the info window
void infoDestroy(GtkWidget *window, gpointer widget) {
	gtk_widget_hide_all(GTK_WIDGET(window));
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(widget), FALSE);
}

gboolean infoDeleteEvent(GtkWidget *window, GdkEvent *event, gpointer widget) {
	infoDestroy(window, widget);

	return TRUE;
}

//"Info" toolbar button handler
void onWindowToggled(GtkWidget *widget, gpointer window) {
	GdkColor bgColor;
	gdk_color_parse("black", &bgColor);
	gtk_widget_modify_bg(GTK_WIDGET(window), GTK_STATE_NORMAL, &bgColor);

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget)))
		gtk_widget_show_all(GTK_WIDGET(window));
	else
		gtk_widget_hide_all(GTK_WIDGET(window));
}

//called when the spin button value is changed
void onRobotIdChanged(GtkWidget *widget, gpointer data) {
	int *viewedRobot = (int*) data;
	int changedRobotId = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

	if (*viewedRobot != changedRobotId)
		*viewedRobot = changedRobotId;

	infoTimeCache = 0;
	infoMessages = 0;
	infoLastSecond = 0;
}

//update the drawing of the robot
void ClientViewer::updateViewer(OwnRobot* ownRobot) {
#ifdef DEBUG
	debug << "Drawing robot: " << viewedRobot/*<<", moving at angle of: "<<ownRobot->angle*/<< endl;
	for (unsigned int i = 0; i < ownRobot->seenRobots.size(); i++) {
		debug << "See robot: " << ownRobot->seenRobots.at(i)->id <<" relatively at (" << ownRobot->seenRobots.at(i)->relx << ", "
				<< ownRobot->seenRobots.at(i)->rely << " )" << endl;
	}

	for (unsigned int i = 0; i < ownRobot->seenPucks.size(); i++) {
		debug << "See puck relatively at ( " << ownRobot->seenPucks.at(i)->relx << ", "
				<< ownRobot->seenPucks.at(i)->rely << " )" << endl;
	}

	debug << endl;
#endif

	ownRobotDraw = *ownRobot;
	draw = true;
	gtk_widget_queue_draw(GTK_WIDGET(drawingArea));
}

//update the info window
void updateInfoWindow(OwnRobot* ownRobotDraw, GtkBuilder* builder) {
	infoTimeCache = time(NULL);
	infoMessages++;

	if (infoTimeCache > infoLastSecond) {
		GtkLabel *labSpeed = GTK_LABEL(gtk_builder_get_object( builder, "labSpeed" ));
		GtkLabel *labAngle = GTK_LABEL(gtk_builder_get_object( builder, "labAngle" ));
		GtkLabel *labPuck = GTK_LABEL(gtk_builder_get_object( builder, "labPuck" ));
		GtkLabel *labFrames = GTK_LABEL(gtk_builder_get_object( builder, "labFrames" ));
		string tmp;

		tmp = "Traveling with a speed of " + helper::toString(ownRobotDraw->vx) + ", " + helper::toString(
				ownRobotDraw->vy) + "( x, y )";
		gtk_label_set_text(labSpeed, tmp.c_str());

		tmp = "Traveling with at an angle of " + helper::toString(ownRobotDraw->angle);
		gtk_label_set_text(labAngle, tmp.c_str());

		if (ownRobotDraw->hasPuck)
			tmp = "Carrying a puck";
		else
			tmp = "NOT carrying a puck";

		if (ownRobotDraw->hasCollided)
			tmp += " and colliding";
		else
			tmp += " and NOT colliding";
		gtk_label_set_text(labPuck, tmp.c_str());

		tmp = helper::toString(infoMessages) + " updates per second";
		gtk_label_set_text(labFrames, tmp.c_str());

		infoMessages = 0;
		infoLastSecond = infoTimeCache;
	}
}

//called on the drawingArea's expose event
gboolean drawingAreaExpose(GtkWidget *widgetDrawingArea, GdkEventExpose *event, gpointer data) {
	passToDrawingAreaExpose* passed = (passToDrawingAreaExpose*) data;
	bool* draw = passed->draw;

	if (*draw) {
		OwnRobot* ownRobotDraw = passed->ownRobotDraw;
		int myTeam = passed->myTeam;
		int numberOfRobots = passed->numberOfRobots;
		int *drawFactor = passed->drawFactor;
		int robotDiameter = passed->robotDiameter;
		int puckDiameter = passed->puckDiameter;
		int viewDistance = passed->viewDistance;
		int imageWidth = (viewDistance + robotDiameter) * (*drawFactor) * 2;
		int imageHeight = (viewDistance + robotDiameter) * (*drawFactor) * 2;

		//origin is the middle point where our viewed robot is located
		int origin[] = { viewDistance * (*drawFactor), viewDistance * (*drawFactor) };

		cairo_t *cr = gdk_cairo_create(GTK_DRAWING_AREA(widgetDrawingArea)->widget.window);
		cairo_set_line_width(cr, 3);

		ColorObject color = colorFromTeam(myTeam);

		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_arc(cr, origin[0], origin[1], robotDiameter * *drawFactor / 2, 0, 2 * M_PI);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgb(cr, color.r, color.g, color.b);
		cairo_fill(cr);

		//ownRobot->angle does not provide a valid angle ( it's always zero )!
		//draw the line showing the angle that the robot is moving at
		/*
		 cairo_move_to(cr, origin[0], origin[1]);
		 cairo_line_to(cr, origin[0], origin[1]+robotDiameter * *drawFactor / 2);
		 cairo_stroke(cr);*/

		for (unsigned int i = 0; i < ownRobotDraw->seenRobots.size(); i++) {
				color = colorFromTeam(ownRobotDraw->seenRobots.at(i)->id / numberOfRobots);
				cairo_set_source_rgb(cr, 1, 1, 1);
				cairo_arc(cr, origin[0] + (ownRobotDraw->seenRobots.at(i)->relx * *drawFactor),
						(origin[1] + ownRobotDraw->seenRobots.at(i)->rely * *drawFactor),
						robotDiameter * *drawFactor / 2, 0, 2 * M_PI);
				cairo_stroke_preserve(cr);

				cairo_set_source_rgb(cr, color.r, color.g, color.b);
				cairo_fill(cr);
		}

		//draw seen pucks
		cairo_set_source_rgb(cr, 0, 0, 0);
		for (unsigned int i = 0; i < ownRobotDraw->seenPucks.size(); i++) {
				cairo_arc(cr, origin[0] + ownRobotDraw->seenPucks.at(i)->relx * *drawFactor,
						origin[1] + ownRobotDraw->seenPucks.at(i)->rely * *drawFactor, puckDiameter * *drawFactor / 2, 0, 2 * M_PI);
				cairo_fill(cr);
		}

		//draw a rectangle around the drawing area
		cairo_rectangle(cr, 0, 0, imageWidth, imageHeight);
		cairo_stroke(cr);

		cairo_destroy(cr);
		*draw = false;

		if (gtk_toggle_tool_button_get_active(passed->info))
			updateInfoWindow(ownRobotDraw, passed->builder);
	}

	return FALSE;
}

//resize the drawing area based on the drawFactor
void resizeByDrawFactor(int drawFactor, int robotDiameter, int viewDistance, GtkDrawingArea *drawingArea,
		GtkWidget *mainWindow) {
	//drawing related variables
	int imageWidth = (viewDistance + robotDiameter) * drawFactor * 2;
	int imageHeight = (viewDistance + robotDiameter) * drawFactor * 2;

#ifdef DEBUG
	cout << "imageWidth is " << imageWidth << " and the imageHeight is " << imageHeight << endl;
#endif

	gtk_widget_set_size_request(GTK_WIDGET(drawingArea), imageWidth, imageHeight);
	gtk_window_resize(GTK_WINDOW(mainWindow), imageWidth, imageHeight);
}

//zoom in button handler
void onZoomInClicked(GtkWidget *widgetDrawingArea, gpointer data) {

	passToZoom *passed = (passToZoom*) data;
	int *drawFactor = passed->drawFactor;

	if (*drawFactor >= MAXZOOMED) {
		gtk_widget_set_sensitive(GTK_WIDGET(passed->zoomIn), false);
	} else {
		GtkWidget *zoomOut = GTK_WIDGET(passed->zoomOut);
		int viewDistance = passed->viewDistance;
		int robotDiameter = passed->robotDiameter;
		*drawFactor = *drawFactor + ZOOMSPEED;

		resizeByDrawFactor(*drawFactor, robotDiameter, viewDistance, passed->drawingArea, passed->mainWindow);

		if (!gtk_widget_get_sensitive(zoomOut) && *drawFactor <= MAXZOOMED)
			gtk_widget_set_sensitive(zoomOut, true);
	}
}

//zoom out button hundler
void onZoomOutClicked(GtkWidget *widgetDrawingArea, gpointer data) {

	passToZoom *passed = (passToZoom*) data;
	int *drawFactor = passed->drawFactor;

	if (*drawFactor <= MINZOOMED) {
		gtk_widget_set_sensitive(GTK_WIDGET(passed->zoomOut), false);
	} else {
		GtkWidget *zoomIn = GTK_WIDGET(passed->zoomIn);
		int viewDistance = passed->viewDistance;
		int robotDiameter = passed->robotDiameter;
		*drawFactor = *drawFactor - ZOOMSPEED;

		resizeByDrawFactor(*drawFactor, robotDiameter, viewDistance, passed->drawingArea, passed->mainWindow);

		if (!gtk_widget_get_sensitive(zoomIn) && *drawFactor <= MAXZOOMED)
			gtk_widget_set_sensitive(zoomIn, true);
	}
}

//initializations and simple modifications for the things that will be drawn
void ClientViewer::initClientViewer(int numberOfRobots, int myTeam, int robotDiameter, int puckDiameter,
		int viewDistance, int _drawFactor) {
#ifdef DEBUG
	debug << "Starting the Client Viewer!" << endl;
#endif
	g_type_init();

	//load the builder file
	gtk_builder_add_from_file(builder, builderPath.c_str(), NULL);
	gtk_builder_connect_signals(builder, NULL);

	GtkWidget *mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	GtkToggleToolButton *info = GTK_TOGGLE_TOOL_BUTTON(gtk_builder_get_object(builder, "Info"));
	GtkToolButton *zoomIn = GTK_TOOL_BUTTON(gtk_builder_get_object(builder, "ZoomIn"));
	GtkToolButton *zoomOut = GTK_TOOL_BUTTON(gtk_builder_get_object(builder, "ZoomOut"));
	GtkWidget *about = GTK_WIDGET(gtk_builder_get_object(builder, "About"));
	GtkWidget *infoWindow = GTK_WIDGET(gtk_builder_get_object(builder, "infoWindow"));
	GtkSpinButton *robotId = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "robotId"));
	GdkColor color;

	//drawing related variables
	drawFactor = new int();
	*drawFactor = _drawFactor;
	int imageWidth = (viewDistance + robotDiameter) * *drawFactor * 2, imageHeight = (viewDistance + robotDiameter)
			* *drawFactor * 2;

#ifdef DEBUG
	debug << "imageWidth is " << imageWidth << " and the imageHeight is " << imageHeight << endl;
#endif
	drawingArea = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "drawingArea"));

	gtk_widget_set_size_request(GTK_WIDGET(drawingArea), imageWidth, imageHeight);
	gtk_window_resize(GTK_WINDOW(mainWindow), imageWidth, imageHeight);

	//set the upper value for the spin button
	gtk_adjustment_set_upper(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "robotIdAdjustment")), numberOfRobots - 1);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_builder_get_object(builder, "robotIdAdjustment")), -1);

	//keep the info window floating on top of the main window
	gtk_window_set_keep_above(GTK_WINDOW(infoWindow), true);

	//change the color of the main window's background to black
	gdk_color_parse("black", &color);
	gtk_widget_modify_bg(GTK_WIDGET(mainWindow), GTK_STATE_NORMAL, &color);
	gdk_color_parse("white", &color);
	gtk_widget_modify_bg(GTK_WIDGET(drawingArea), GTK_STATE_NORMAL, &color);

	g_signal_connect(robotId, "value-changed", G_CALLBACK(onRobotIdChanged), (gpointer) & viewedRobot);

	g_signal_connect(info, "toggled", G_CALLBACK(onWindowToggled), (gpointer) infoWindow);
	g_signal_connect(about, "clicked", G_CALLBACK(onAboutClicked), (gpointer) mainWindow);

	g_signal_connect(zoomIn, "clicked", G_CALLBACK(onZoomInClicked), (gpointer)(new passToZoom(viewDistance, robotDiameter,
							drawFactor, mainWindow, drawingArea, zoomIn, zoomOut)));
	g_signal_connect(zoomOut, "clicked", G_CALLBACK(onZoomOutClicked), (gpointer)(new passToZoom(viewDistance, robotDiameter,
							drawFactor, mainWindow, drawingArea, zoomIn, zoomOut)));

	g_signal_connect(infoWindow, "destroy", G_CALLBACK(infoDestroy), (gpointer) info);
	g_signal_connect(infoWindow, "delete-event", G_CALLBACK(infoDeleteEvent), (gpointer) info);

	g_signal_connect(drawingArea, "expose-event", G_CALLBACK(drawingAreaExpose), (gpointer)(
					new passToDrawingAreaExpose(myTeam, numberOfRobots, drawFactor, robotDiameter, puckDiameter, viewDistance, &draw, &ownRobotDraw, info, builder)));

	gtk_widget_show_all(mainWindow);
}

ClientViewer::ClientViewer(char* argv) :
	viewedRobot(-1), draw(false) {

	//assume that the clientviewer.builder is in the same directory as the executable that we are running
	builderPath = argv;
	builderPath = builderPath.substr(0, builderPath.find_last_of("//") + 1) + "clientviewer.glade";
	builder = gtk_builder_new();

#ifdef DEBUG
	debug.open(helper::clientViewerDebugLogName.c_str(), ios::out);
#endif
}

ClientViewer::~ClientViewer() {
	g_object_unref(G_OBJECT(builder));
#ifdef DEBUG
	debug.close();
#endif
}
