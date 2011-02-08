// gui.cpp
#include <GL/glut.h>

#include "gui.h"
#include "logviewer.h"

Logviewer* Gui::_logviewer = 0;

void Gui::setLogviewer(Logviewer* logviewer) {
  _logviewer = logviewer;
}

void Gui::idle_func(void) {
  Logviewer::updateTimestep();
}

void Gui::timer_func(int dummy) {
  glutPostRedisplay(); // force redraw
}

// draw the world - this is called whenever the window needs redrawn
void Gui::display_func(void) {  
  Logviewer::winsize = glutGet( GLUT_WINDOW_WIDTH );
  glClear( GL_COLOR_BUFFER_BIT );  
  Gui::drawAll();
  glutSwapBuffers();
	
  // run this function again in about 50 msec
  glutTimerFunc( 20, timer_func, 0 );
}

void Gui::mouse_func(int button, int state, int x, int y) {  
  /*if( (button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN ) )
	 {
		Robot::paused = !Robot::paused;
	 }
   */
}

void Gui::glDrawCircle(double x, double y, double r, double count) {
	glBegin(GL_LINE_LOOP);
	for( float a=0; a<(M_PI*2.0); a+=M_PI/count )
		glVertex2f( x + sin(a) * r, y + cos(a) * r );
	glEnd();
}

//
// Robot static member methods ---------------------------------------------

void Gui::initGraphics(int argc, char* argv[]) {
  // initialize opengl graphics
  glutInit( &argc, argv );
  glutInitWindowSize( Logviewer::winsize, Logviewer::winsize );
  glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA );
  glutCreateWindow( argv[0] ); // program name
  glClearColor( 0.1,0.1,0.1,1 ); // dark grey
  glutDisplayFunc( display_func );
  glutTimerFunc( 50, timer_func, 0 );
  glutMouseFunc( mouse_func );
  glutIdleFunc( idle_func );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_BLEND );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  gluOrtho2D( 0,1,0,1 );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glScalef( 1.0/Logviewer::worldsize, 1.0/Logviewer::worldsize, 1 ); 
	glPointSize( 4.0 );
}

void Gui::updateGui() {
	glutMainLoop();
}

// render all robots in OpenGL
void Gui::drawAll() {		
	FOR_EACH( r, _logviewer->robots )
		draw(*r);
	
	FOR_EACH( it, _logviewer->homes )
		{
			Home* h = *it;
			
			glColor3f( 1, 1, 1);

			glDrawCircle( h->_x, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x+Logviewer::worldsize, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x-Logviewer::worldsize, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x, h->_y+Logviewer::worldsize, h->_radius, 16 );
			glDrawCircle( h->_x, h->_y-Logviewer::worldsize, h->_radius, 16 );
		}
	
	glColor3f( 1,1,1 ); // green
	glBegin( GL_POINTS );
	FOR_EACH( p, _logviewer->puckStacks )
		glVertex2f( (*p)->_x, (*p)->_y );
	glEnd();
}

// draw a robot
void Gui::draw(Robot* r) {
  glPushMatrix();
	// shift into this robot's local coordinate frame
  glTranslatef( r->_x, r->_y, 0 );
  glRotatef( Logviewer::rtod(r->_angle), 0,0,1 );
  
	glColor3f( 1, 1, 1); 
	
	double radius = .01; //Robot::radius;
	
	// if robots are smaller than 4 pixels across, draw them as points
	if( (radius * (double)Logviewer::winsize/(double)Logviewer::worldsize) < 2.0 )
	  {
		 glBegin( GL_POINTS );
		 glVertex2f( 0,0 );
		 glEnd();
	  }
	else
	  {
		 // draw a circular body
		 glBegin(GL_LINE_LOOP);
		 for( float a=0; a<(M_PI*2.0); a+=M_PI/16 )
			glVertex2f( sin(a) * radius, 
							cos(a) * radius );
		 glEnd();
		 
		 // draw a nose indicating forward direction
		 glBegin(GL_LINES);
		 glVertex2f( 0, 0 );
		 glVertex2f( radius, 0 );
		 glEnd();
	  }

  /*if( Robot::show_data )
	 {
		glColor3f( 1,0,0 ); // red
		
		FOR_EACH( it, see_robots )
		  {
				float dx = it->range * cos(it->bearing);
				float dy = it->range * sin(it->bearing);
				
				glBegin( GL_LINES );
				glVertex2f( 0,0 );
				glVertex2f( dx, dy );
				glEnd();
		  }
		
		glColor3f( 0.3,0.8,0.3 ); // light green
		
		FOR_EACH( it, see_pucks )
		  {
				float dx = it->range * cos(it->bearing);
				float dy = it->range * sin(it->bearing);
				
				glBegin( GL_LINES );
				glVertex2f( 0,0 );
				glVertex2f( dx, dy );
				glEnd();
		  }
		
		glColor3f( 0.4,0.4,0.4 ); // grey

		// draw the sensor FOV
		glBegin(GL_LINE_LOOP);
		
		glVertex2f( 0, 0 );
		
		double right = -fov/2.0;
		double left = +fov/2.0;// + M_PI;
		double incr = fov/32.0;
		for( float a=right; a<left; a+=incr)
		  glVertex2f( cos(a) * range, 
						  sin(a) * range );

		glVertex2f( cos(left) * range, 
						sin(left) * range );
		
		glEnd();		
	 }*/
	
	// shift out of local coordinate frame
  glPopMatrix();
}
